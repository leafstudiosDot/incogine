#include "anim_io.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <utility>

#include "../engine/version.h"
#include "anim_json.h"

namespace icg {
namespace anim {
namespace {

using json::JsonArray;
using json::JsonObject;
using json::JsonValue;

// Rounds to 4 decimals so a save/load round-trip is bit-identical and diffs
// stay free of float noise. Rounding happens in double, not float: a float
// cannot hold 0.1 exactly, so rounding at float precision would emit
// "0.10000000149011612" and defeat the whole point.
double Round4(double value) {
    return std::nearbyint(value * 10000.0) / 10000.0;
}

JsonValue Num(float value) {
    return JsonValue::Number(Round4(static_cast<double>(value)));
}

float ReadFloat(const JsonValue& parent, const char* key, float fallback) {
    const JsonValue* found = parent.Find(key);
    return found != nullptr ? static_cast<float>(found->AsNumber(fallback))
                            : fallback;
}

int ReadInt(const JsonValue& parent, const char* key, int fallback) {
    const JsonValue* found = parent.Find(key);
    return found != nullptr ? static_cast<int>(found->AsInt(fallback)) : fallback;
}

bool ReadBool(const JsonValue& parent, const char* key, bool fallback) {
    const JsonValue* found = parent.Find(key);
    return found != nullptr ? found->AsBool(fallback) : fallback;
}

std::string ReadString(const JsonValue& parent, const char* key,
                       const std::string& fallback) {
    const JsonValue* found = parent.Find(key);
    return found != nullptr ? found->AsStringOr(fallback) : fallback;
}

// --- writers --

JsonValue WriteTransform(const AnimTransform& transform) {
    JsonObject obj;
    obj.emplace_back("x", Num(transform.position.x));
    obj.emplace_back("y", Num(transform.position.y));
    obj.emplace_back("rotation", Num(transform.rotation));
    obj.emplace_back("scaleX", Num(transform.scale.x));
    obj.emplace_back("scaleY", Num(transform.scale.y));
    obj.emplace_back("skewX", Num(transform.skewX));
    obj.emplace_back("skewY", Num(transform.skewY));
    obj.emplace_back("alpha", Num(transform.alpha));
    obj.emplace_back("color", JsonValue::String(transform.colorTransform.ToHex()));
    return JsonValue::Object(std::move(obj));
}

AnimTransform ReadTransform(const JsonValue* value) {
    AnimTransform transform;
    if (value == nullptr || !value->IsObject()) {
        return transform;
    }
    transform.position = Vec2(ReadFloat(*value, "x", 0.0f),
                              ReadFloat(*value, "y", 0.0f));
    transform.rotation = ReadFloat(*value, "rotation", 0.0f);
    transform.scale = Vec2(ReadFloat(*value, "scaleX", 1.0f),
                           ReadFloat(*value, "scaleY", 1.0f));
    transform.skewX = ReadFloat(*value, "skewX", 0.0f);
    transform.skewY = ReadFloat(*value, "skewY", 0.0f);
    transform.alpha = ReadFloat(*value, "alpha", 1.0f);
    transform.colorTransform = AnimColor::FromHex(ReadString(*value, "color", "#FFFFFFFF"));
    return transform;
}

const char* SegmentKindName(AnimSegment::Kind kind) {
    switch (kind) {
        case AnimSegment::Kind::Move: return "M";
        case AnimSegment::Kind::Line: return "L";
        case AnimSegment::Kind::Cubic: return "C";
        case AnimSegment::Kind::Close: return "Z";
    }
    return "L";
}

AnimSegment::Kind SegmentKindFromName(const std::string& name,
                                      AnimSegment::Kind fallback) {
    if (name == "M") return AnimSegment::Kind::Move;
    if (name == "L") return AnimSegment::Kind::Line;
    if (name == "C") return AnimSegment::Kind::Cubic;
    if (name == "Z") return AnimSegment::Kind::Close;
    return fallback;
}

JsonValue WritePath(const AnimPath& path) {
    JsonArray out;
    out.reserve(path.segments.size());
    for (const AnimSegment& segment : path.segments) {
        JsonObject obj;
        obj.emplace_back("k", JsonValue::String(SegmentKindName(segment.kind)));
        switch (segment.kind) {
            case AnimSegment::Kind::Move:
            case AnimSegment::Kind::Line: {
                JsonArray point;
                point.emplace_back(Num(segment.p[0].x));
                point.emplace_back(Num(segment.p[0].y));
                obj.emplace_back("p", JsonValue::Array(std::move(point)));
                break;
            }
            case AnimSegment::Kind::Cubic: {
                JsonArray points;
                for (int i = 0; i < 3; ++i) {
                    points.emplace_back(Num(segment.p[i].x));
                    points.emplace_back(Num(segment.p[i].y));
                }
                obj.emplace_back("p", JsonValue::Array(std::move(points)));
                break;
            }
            case AnimSegment::Kind::Close:
                break;
        }
        out.emplace_back(JsonValue::Object(std::move(obj)));
    }
    return JsonValue::Array(std::move(out));
}

AnimPath ReadPath(const JsonValue* value) {
    AnimPath path;
    if (value == nullptr || !value->IsArray()) {
        return path;
    }
    const JsonArray& items = value->AsArray();
    path.segments.reserve(items.size());
    for (size_t i = 0; i < items.size(); ++i) {
        const JsonValue* item = items[i].Find("k");
        if (item == nullptr || !item->IsString()) {
            continue;
        }
        AnimSegment segment(
            SegmentKindFromName(item->AsString(), AnimSegment::Kind::Line));
        const JsonValue* points = items[i].Find("p");
        if (points != nullptr && points->IsArray()) {
            const JsonArray& coords = points->AsArray();
            // Flat [x, y, x, y, ...] triples; guard against a short array.
            const size_t needed = segment.kind == AnimSegment::Kind::Cubic
                                      ? 3
                                      : 1;
            for (size_t c = 0; c < needed; ++c) {
                const size_t base = c * 2;
                if (base + 1 >= coords.size()) {
                    break;
                }
                segment.p[c] = Vec2(static_cast<float>(coords[base].AsNumber()),
                                    static_cast<float>(coords[base + 1].AsNumber()));
            }
        }
        path.segments.push_back(segment);
    }
    return path;
}

JsonValue WriteStyle(const AnimStyle& style) {
    JsonObject obj;
    obj.emplace_back("fill", JsonValue::Bool(style.hasFill));
    obj.emplace_back("fillColor", JsonValue::String(style.fill.ToHex()));
    obj.emplace_back("stroke", JsonValue::Bool(style.hasStroke));
    obj.emplace_back("strokeColor", JsonValue::String(style.stroke.ToHex()));
    obj.emplace_back("strokeWidth", Num(style.strokeWidth));
    obj.emplace_back("cap", JsonValue::Int(static_cast<long long>(style.cap)));
    obj.emplace_back("join", JsonValue::Int(static_cast<long long>(style.join)));
    return JsonValue::Object(std::move(obj));
}

AnimStyle ReadStyle(const JsonValue* value) {
    AnimStyle style;
    if (value == nullptr || !value->IsObject()) {
        return style;
    }
    style.hasFill = ReadBool(*value, "fill", style.hasFill);
    style.fill = AnimColor::FromHex(ReadString(*value, "fillColor", "#FFFFFFFF"));
    style.hasStroke = ReadBool(*value, "stroke", style.hasStroke);
    style.stroke = AnimColor::FromHex(ReadString(*value, "strokeColor", "#000000FF"));
    style.strokeWidth = ReadFloat(*value, "strokeWidth", style.strokeWidth);
    style.cap = static_cast<LineCap>(ReadInt(*value, "cap", static_cast<int>(style.cap)));
    style.join = static_cast<LineJoin>(ReadInt(*value, "join", static_cast<int>(style.join)));
    return style;
}

JsonValue WriteTween(const TweenSpan& span) {
    JsonObject obj;
    const char* typeName = "none";
    switch (span.type) {
        case TweenType::None: typeName = "none"; break;
        case TweenType::Motion: typeName = "motion"; break;
        case TweenType::Shape: typeName = "shape"; break;
    }
    obj.emplace_back("type", JsonValue::String(typeName));

    JsonObject easing;
    easing.emplace_back("kind", JsonValue::Int(static_cast<long long>(span.easing.kind)));
    easing.emplace_back("p1x", Num(span.easing.p1x));
    easing.emplace_back("p1y", Num(span.easing.p1y));
    easing.emplace_back("p2x", Num(span.easing.p2x));
    easing.emplace_back("p2y", Num(span.easing.p2y));
    obj.emplace_back("easing", JsonValue::Object(std::move(easing)));

    // Named flags keep the file readable and let the schema grow a flag without
    // breaking old readers (unknown names are ignored on load).
    JsonArray flags;
    if (span.motionFlags & kMotionPosition) flags.emplace_back(JsonValue::String("position"));
    if (span.motionFlags & kMotionScale) flags.emplace_back(JsonValue::String("scale"));
    if (span.motionFlags & kMotionRotation) flags.emplace_back(JsonValue::String("rotation"));
    if (span.motionFlags & kMotionAlpha) flags.emplace_back(JsonValue::String("alpha"));
    if (span.motionFlags & kMotionColor) flags.emplace_back(JsonValue::String("color"));
    obj.emplace_back("motionFlags", JsonValue::Array(std::move(flags)));
    obj.emplace_back("shapeHints", JsonValue::Bool(span.shapeHints));
    return JsonValue::Object(std::move(obj));
}

TweenSpan ReadTween(const JsonValue* value) {
    TweenSpan span;
    if (value == nullptr || !value->IsObject()) {
        return span;
    }
    const std::string typeName = ReadString(*value, "type", "none");
    if (typeName == "motion") {
        span.type = TweenType::Motion;
    } else if (typeName == "shape") {
        span.type = TweenType::Shape;
    } else {
        span.type = TweenType::None;
    }

    const JsonValue* easing = value->Find("easing");
    if (easing != nullptr && easing->IsObject()) {
        span.easing.kind = static_cast<EasingKind>(
            ReadInt(*easing, "kind", static_cast<int>(EasingKind::Linear)));
        span.easing.p1x = ReadFloat(*easing, "p1x", 0.0f);
        span.easing.p1y = ReadFloat(*easing, "p1y", 0.0f);
        span.easing.p2x = ReadFloat(*easing, "p2x", 1.0f);
        span.easing.p2y = ReadFloat(*easing, "p2y", 1.0f);
    }

    const JsonValue* flags = value->Find("motionFlags");
    if (flags != nullptr && flags->IsArray()) {
        uint8_t mask = 0;
        for (const JsonValue& flag : flags->AsArray()) {
            const std::string name = flag.AsString();
            if (name == "position") mask |= kMotionPosition;
            else if (name == "scale") mask |= kMotionScale;
            else if (name == "rotation") mask |= kMotionRotation;
            else if (name == "alpha") mask |= kMotionAlpha;
            else if (name == "color") mask |= kMotionColor;
        }
        span.motionFlags = mask;
    }
    span.shapeHints = ReadBool(*value, "shapeHints", span.shapeHints);
    return span;
}

JsonValue WriteShape(const AnimShape& shape) {
    JsonObject obj;
    obj.emplace_back("id", JsonValue::Int(static_cast<long long>(shape.id)));
    obj.emplace_back("name", JsonValue::String(shape.name));
    obj.emplace_back("path", WritePath(shape.path));
    obj.emplace_back("style", WriteStyle(shape.style));
    obj.emplace_back("transform", WriteTransform(shape.transform));
    return JsonValue::Object(std::move(obj));
}

AnimShape ReadShape(const JsonValue* value) {
    AnimShape shape;
    if (value == nullptr || !value->IsObject()) {
        return shape;
    }
    shape.id = static_cast<uint64_t>(ReadInt(*value, "id", 0));
    shape.name = ReadString(*value, "name", "");
    shape.path = ReadPath(value->Find("path"));
    shape.style = ReadStyle(value->Find("style"));
    shape.transform = ReadTransform(value->Find("transform"));
    return shape;
}

} // namespace

// ---------------------------------------------------------------- migrate --

bool Migrate(JsonValue& root, int fromVersion, std::string& error) {
    if (fromVersion < 1) {
        error = "Unsupported .incoanim formatVersion " +
                std::to_string(fromVersion) + " (this build understands v1 and later).";
        return false;
    }
    if (fromVersion > kIncoanimFormatVersion) {
        error = "This .incoanim was saved by a newer Incogine (formatVersion " +
                std::to_string(fromVersion) + "; this build understands v" +
                std::to_string(kIncoanimFormatVersion) +
                "). Update Incogine to open it.";
        return false;
    }
    // Upgrade chain goes here, one case per historical version:
    //   switch (fromVersion) {
    //     case 1: /* v1 -> v2 */ fromVersion = 2; break;
    //     default: break;
    //   }
    // v1 is current, so nothing to do.
    return true;
}

// -------------------------------------------------------------- serialize --

std::string Serialize(const AnimDocument& document) {
    JsonObject root;

    root.emplace_back("formatVersion", JsonValue::Int(kIncoanimFormatVersion));
    root.emplace_back("generator",
                      JsonValue::String(std::string("Incogine Animate ") +
                                        VERSION_STRING));

    JsonObject stage;
    stage.emplace_back("width", JsonValue::Int(document.stageWidth));
    stage.emplace_back("height", JsonValue::Int(document.stageHeight));
    stage.emplace_back("fps", JsonValue::Int(document.fps));
    stage.emplace_back("background", JsonValue::String(document.background.ToHex()));
    stage.emplace_back("transparentBackground",
                       JsonValue::Bool(document.transparentBackground));

    JsonObject timeline;
    timeline.emplace_back("lengthFrames", JsonValue::Int(document.lengthFrames));
    timeline.emplace_back("loop", JsonValue::Bool(document.loop));

    JsonObject doc;
    doc.emplace_back("stage", JsonValue::Object(std::move(stage)));
    doc.emplace_back("timeline", JsonValue::Object(std::move(timeline)));
    doc.emplace_back("bakeScale", Num(document.bakeScale));
    root.emplace_back("document", JsonValue::Object(std::move(doc)));

    JsonArray layers;
    layers.reserve(document.layers.size());
    for (const AnimLayer& layer : document.layers) {
        JsonObject layerObj;
        layerObj.emplace_back("id", JsonValue::Int(static_cast<long long>(layer.id)));
        layerObj.emplace_back("name", JsonValue::String(layer.name));
        layerObj.emplace_back("visible", JsonValue::Bool(layer.visible));
        layerObj.emplace_back("locked", JsonValue::Bool(layer.locked));

        JsonArray keyframes;
        keyframes.reserve(layer.frames.size());
        for (const AnimKeyframe& key : layer.frames) {
            JsonObject keyObj;
            keyObj.emplace_back("frame", JsonValue::Int(key.frame));
            keyObj.emplace_back("kind",
                                JsonValue::String(key.kind == KeyframeKind::Blank
                                                      ? "blank"
                                                      : "key"));
            keyObj.emplace_back("transform", WriteTransform(key.transform));
            keyObj.emplace_back("tweenIn", WriteTween(key.tweenIn));

            JsonArray shapes;
            shapes.reserve(key.shapes.size());
            for (const AnimShape& shape : key.shapes) {
                shapes.emplace_back(WriteShape(shape));
            }
            keyObj.emplace_back("shapes", JsonValue::Array(std::move(shapes)));
            keyframes.emplace_back(JsonValue::Object(std::move(keyObj)));
        }
        layerObj.emplace_back("keyframes", JsonValue::Array(std::move(keyframes)));
        layers.emplace_back(JsonValue::Object(std::move(layerObj)));
    }
    root.emplace_back("layers", JsonValue::Array(std::move(layers)));

    return json::Write(JsonValue::Object(std::move(root)));
}

// ------------------------------------------------------------ deserialize --

bool Deserialize(const std::string& text, AnimDocument& out, std::string& error) {
    JsonValue root;
    std::string parseError;
    if (!json::Parse(text, root, parseError)) {
        error = "Not valid JSON: " + parseError;
        return false;
    }
    if (!root.IsObject()) {
        error = "Top level of an .incoanim file must be a JSON object.";
        return false;
    }

    const JsonValue* versionValue = root.Find("formatVersion");
    if (versionValue == nullptr) {
        error = "Missing required field \"formatVersion\".";
        return false;
    }
    const int version = static_cast<int>(versionValue->AsInt(0));
    if (!Migrate(root, version, error)) {
        return false;
    }

    const JsonValue* docValue = root.Find("document");
    if (docValue == nullptr || !docValue->IsObject()) {
        error = "Missing required field \"document\".";
        return false;
    }

    AnimDocument document;
    const JsonValue* stage = docValue->Find("stage");
    if (stage != nullptr) {
        document.stageWidth = ReadInt(*stage, "width", document.stageWidth);
        document.stageHeight = ReadInt(*stage, "height", document.stageHeight);
        document.fps = ReadInt(*stage, "fps", document.fps);
        document.background = AnimColor::FromHex(
            ReadString(*stage, "background", "#00000000"));
        document.transparentBackground =
            ReadBool(*stage, "transparentBackground", document.transparentBackground);
    }
    const JsonValue* timeline = docValue->Find("timeline");
    if (timeline != nullptr) {
        document.lengthFrames = ReadInt(*timeline, "lengthFrames", 1);
        document.loop = ReadBool(*timeline, "loop", true);
    }
    document.bakeScale = ReadFloat(*docValue, "bakeScale", 1.0f);

    const JsonValue* layers = root.Find("layers");
    if (layers != nullptr && layers->IsArray()) {
        for (const JsonValue& layerValue : layers->AsArray()) {
            AnimLayer layer;
            layer.id = static_cast<uint64_t>(ReadInt(layerValue, "id", 0));
            layer.name = ReadString(layerValue, "name", "");
            layer.visible = ReadBool(layerValue, "visible", true);
            layer.locked = ReadBool(layerValue, "locked", false);

            const JsonValue* keyframes = layerValue.Find("keyframes");
            if (keyframes != nullptr && keyframes->IsArray()) {
                for (const JsonValue& keyValue : keyframes->AsArray()) {
                    AnimKeyframe key;
                    key.frame = ReadInt(keyValue, "frame", 1);
                    key.kind = ReadString(keyValue, "kind", "key") == "blank"
                                   ? KeyframeKind::Blank
                                   : KeyframeKind::Key;
                    key.transform = ReadTransform(keyValue.Find("transform"));
                    key.tweenIn = ReadTween(keyValue.Find("tweenIn"));
                    const JsonValue* shapes = keyValue.Find("shapes");
                    if (shapes != nullptr && shapes->IsArray()) {
                        key.shapes.reserve(shapes->AsArray().size());
                        for (const JsonValue& shapeValue : shapes->AsArray()) {
                            key.shapes.push_back(ReadShape(&shapeValue));
                        }
                    }
                    layer.frames.push_back(std::move(key));
                }
            }
            document.layers.push_back(std::move(layer));
        }
    }

    // Normalize last: repairs sorted keyframes, clamps the stage, and moves
    // nextId past every id in the file so later edits cannot collide.
    document.Normalize();
    out = std::move(document);
    return true;
}

// ---------------------------------------------------------------- file IO --

bool LoadFile(const std::string& path, AnimDocument& out, std::string& error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Cannot open " + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return Deserialize(buffer.str(), out, error);
}

bool SaveFile(const std::string& path, const AnimDocument& document,
              std::string& error) {
    // Write to a sibling temp file and rename over the target, so a crash or a
    // cancelled save cannot leave a half-written animation behind.
    const std::string tempPath = path + ".tmp";
    {
        std::ofstream stream(tempPath, std::ios::binary | std::ios::trunc);
        if (!stream) {
            error = "Cannot write " + tempPath;
            return false;
        }
        const std::string text = Serialize(document);
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream) {
            error = "Write failed for " + tempPath;
            return false;
        }
    }
    std::remove(path.c_str());
    if (std::rename(tempPath.c_str(), path.c_str()) != 0) {
        std::remove(tempPath.c_str());
        error = "Cannot replace " + path;
        return false;
    }
    return true;
}

} // namespace anim
} // namespace icg