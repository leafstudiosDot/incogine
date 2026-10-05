// Incogine  Easset importer registry tests.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// The importer seam is what future 3D importers (FBX, OBJ, glTF, .blend) will
// register through, so its lookup and extension rules are worth pinning down.

#include "test_check.h"

#include <memory>
#include <string>

#include "animation/anim2d_importer.h"
#include "animation/anim_io.h"
#include "assets/assetimport.h"

namespace assets = icg::assets;
using icg::assets::AssetImporterRegistry;
using icg::assets::IAssetImporter;

// A stub importer so registry behaviour can be tested without depending on the
// animation module's parse path.
class StubImporter : public IAssetImporter {
public:
    StubImporter(const char* extension, const char* displayName,
                 assets::AssetKind kind)
        : extension_(extension), displayName_(displayName), kind_(kind) {}

    assets::AssetKind kind() const override { return kind_; }
    const char* extension() const override { return extension_; }
    const char* displayName() const override { return displayName_; }

    bool ImportBytes(const void* bytes, size_t size, std::string& error) override {
        lastSize_ = size;
        if (size == 0) {
            error = "empty payload";
            return false;
        }
        imported_ = true;
        error.clear();
        return true;
    }

    bool imported() const { return imported_; }
    size_t lastSize() const { return lastSize_; }

private:
    const char* extension_;
    const char* displayName_;
    assets::AssetKind kind_;
    bool imported_ = false;
    size_t lastSize_ = 0;
};

static void TestExtensionNormalization() {
    TEST_GROUP("extension normalization");

    CHECK_EQ(AssetImporterRegistry::NormalizeExtension("incoanim"),
             std::string("incoanim"));
    CHECK_EQ(AssetImporterRegistry::NormalizeExtension(".incoanim"),
             std::string("incoanim"));
    CHECK_EQ(AssetImporterRegistry::NormalizeExtension(".INCOANIM"),
             std::string("incoanim"));
    CHECK_EQ(AssetImporterRegistry::NormalizeExtension("IncoAnim"),
             std::string("incoanim"));
    CHECK_EQ(AssetImporterRegistry::NormalizeExtension(""), std::string());
    CHECK_EQ(AssetImporterRegistry::NormalizeExtension("."), std::string());
    CHECK_EQ(AssetImporterRegistry::NormalizeExtension(nullptr), std::string());

    CHECK_EQ(AssetImporterRegistry::ExtensionOf("art/hero.incoanim"),
             std::string("incoanim"));
    CHECK_EQ(AssetImporterRegistry::ExtensionOf("hero.incoanim"),
             std::string("incoanim"));
    // A dot in a directory is not an extension.
    CHECK_EQ(AssetImporterRegistry::ExtensionOf("dir.v2/hero"),
             std::string());
    CHECK_EQ(AssetImporterRegistry::ExtensionOf("a/b/c/hero.INCOANIM"),
             std::string("incoanim"));
    // Studio passes native paths.
    CHECK_EQ(AssetImporterRegistry::ExtensionOf("art\\hero.incoanim"),
             std::string("incoanim"));
    // A separator resets the search for a dot.
    CHECK_EQ(AssetImporterRegistry::ExtensionOf("dir.v2/hero.png"),
             std::string("png"));
    CHECK_EQ(AssetImporterRegistry::ExtensionOf("noextension"),
             std::string());
    // A dotfile has no extension.
    CHECK_EQ(AssetImporterRegistry::ExtensionOf("dir/.hidden"),
             std::string());
    CHECK_EQ(AssetImporterRegistry::ExtensionOf(nullptr), std::string());
    // Trailing dot yields an empty extension.
    CHECK_EQ(AssetImporterRegistry::ExtensionOf("trailing."),
             std::string());
}

static void TestRegistration() {
    TEST_GROUP("registration");

    AssetImporterRegistry registry;
    registry.Clear(); // isolate from any earlier case

    // A null importer or empty extension is rejected outright.
    CHECK(!registry.Register(nullptr));
    CHECK(!registry.Register(std::unique_ptr<IAssetImporter>(
        new StubImporter("", "Empty", assets::AssetKind::Unknown))));

    CHECK(registry.Register(std::unique_ptr<IAssetImporter>(
        new StubImporter("stub", "Stub", assets::AssetKind::Binary))));
    // First registration wins: a duplicate extension is refused rather than
    // silently shadowing the earlier importer.
    CHECK(!registry.Register(std::unique_ptr<IAssetImporter>(
        new StubImporter("stub", "Other Stub", assets::AssetKind::Text))));
    // Case and a leading dot do not make it a distinct extension.
    CHECK(!registry.Register(std::unique_ptr<IAssetImporter>(
        new StubImporter(".STUB", "Third", assets::AssetKind::Text))));

    CHECK_EQ(registry.All().size(), static_cast<size_t>(1));
    CHECK(registry.FindByExtension("stub") != nullptr);
    CHECK(registry.FindByExtension(".STUB") != nullptr);
    CHECK(registry.FindByExtension("Stub") != nullptr);
    CHECK(registry.Find("art/thing.stub") != nullptr);
    CHECK(registry.FindByExtension("missing") == nullptr);
    CHECK(registry.Find("art/thing.png") == nullptr);
    CHECK(registry.FindByExtension(nullptr) == nullptr);

    // Registration order is preserved, which is what the asset browser lists.
    CHECK(registry.Register(std::unique_ptr<IAssetImporter>(
        new StubImporter("alpha", "Alpha", assets::AssetKind::Binary))));
    CHECK(registry.Register(std::unique_ptr<IAssetImporter>(
        new StubImporter("beta", "Beta", assets::AssetKind::Binary))));
    REQUIRE_EQ(registry.All().size(), static_cast<size_t>(3));
    CHECK_EQ(std::string(registry.All()[0]->extension()), std::string("stub"));
    CHECK_EQ(std::string(registry.All()[1]->extension()), std::string("alpha"));
    CHECK_EQ(std::string(registry.All()[2]->extension()), std::string("beta"));

    // The stub's ImportBytes round-trips through the interface, proving the
    // bytes-in contract and that a non-empty payload succeeds.
    StubImporter* stub = dynamic_cast<StubImporter*>(registry.Find("x.stub"));
    REQUIRE(stub != nullptr);
    std::string error;
    const char payload[4] = {'a', 'b', 'c', 'd'};
    CHECK(stub->ImportBytes(payload, sizeof(payload), error));
    CHECK(stub->imported());
    CHECK_EQ(stub->lastSize(), sizeof(payload));
    CHECK(error.empty());
    // An empty payload fails with a reason, and the interface is reusable after.
    CHECK(!stub->ImportBytes(payload, 0, error));
    CHECK(!error.empty());
    CHECK(stub->ImportBytes(payload, sizeof(payload), error));
}

static void TestBuiltinsAndAnimationImporter() {
    TEST_GROUP("builtins + .incoanim importer");

    AssetImporterRegistry registry;
    registry.RegisterBuiltins();

    // The built-in .incoanim importer is discoverable by path, extension, and
    // normalized casing.
    IAssetImporter* found = registry.Find("assets/hero.incoanim");
    REQUIRE(found != nullptr);
    CHECK(found == registry.FindByExtension(".INCOANIM"));
    CHECK(found->kind() == assets::AssetKind::Animation2D);
    CHECK_EQ(std::string(found->extension()), std::string("incoanim"));
    CHECK_EQ(std::string(found->displayName()), std::string("2D Animation"));

    icg::anim::Anim2DImporter* anim =
        dynamic_cast<icg::anim::Anim2DImporter*>(found);
    REQUIRE(anim != nullptr);

    // A real document parses through the generic interface.
    icg::anim::AnimDocument doc = icg::anim::AnimDocument::New(800, 600, 30);
    doc.lengthFrames = 12;
    doc.layers[0].name = "Art";
    const std::string text = icg::anim::Serialize(doc);

    std::string error;
    REQUIRE(found->ImportBytes(text.data(), text.size(), error));
    CHECK(error.empty());
    CHECK_EQ(anim->document().stageWidth, 800);
    CHECK_EQ(anim->document().stageHeight, 600);
    CHECK_EQ(anim->document().fps, 30);
    CHECK_EQ(anim->document().lengthFrames, 12);
    REQUIRE_EQ(anim->document().layers.size(), static_cast<size_t>(1));
    CHECK_EQ(anim->document().layers[0].name, std::string("Art"));

    // Malformed input fails with a reason, and the importer must not leave the
    // previous document in place, or a caller would silently read stale art.
    const std::string previousStage =
        std::to_string(anim->document().stageWidth);
    CHECK(!found->ImportBytes("this is not json", 16, error));
    CHECK(!error.empty());
    CHECK(error.find("JSON") != std::string::npos);
    // A failed import must not leave the PREVIOUS document readable, or a
    // caller that ignores the return value would keep drawing stale artwork.
    // The document resets to a fresh default instead.
    CHECK(std::to_string(anim->document().stageWidth) != previousStage);
    CHECK_EQ(anim->document().stageWidth, 1920); // fresh default, not 800
    CHECK(anim->document().layers.empty());

    // A JSON document that is not an .incoanim is rejected too.
    CHECK(!found->ImportBytes("{\"unrelated\": true}", 19, error));
    CHECK(error.find("formatVersion") != std::string::npos);

    // Empty and null payloads are handled without crashing.
    CHECK(!found->ImportBytes("", 0, error));
    CHECK(!found->ImportBytes(nullptr, 4, error));

    // Registering the same built-in twice is refused (RegisterBuiltins is
    // idempotent from the caller's perspective because of this).
    CHECK(!registry.Register(std::unique_ptr<IAssetImporter>(
        new icg::anim::Anim2DImporter())));

    // The process-wide singleton exposes the same built-ins.
    icg::assets::AssetImporterRegistry& shared =
        AssetImporterRegistry::Instance();
    shared.RegisterBuiltins();
    shared.RegisterBuiltins(); // repeat is a no-op, not a duplicate
    REQUIRE(shared.Find("x.incoanim") != nullptr);
    int animationImporters = 0;
    for (IAssetImporter* importer : shared.All()) {
        if (importer->kind() == assets::AssetKind::Animation2D) {
            ++animationImporters;
        }
    }
    CHECK_EQ(animationImporters, 1);
}

int main() {
    std::printf("Incogine asset importer tests\n");
    TestExtensionNormalization();
    TestRegistration();
    TestBuiltinsAndAnimationImporter();
    return ::icgtest::Report("assets");
}
