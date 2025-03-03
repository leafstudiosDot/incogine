import os
import sys

def generate_info_plist(display_name, bundle_name, bundle_identifier, bundle_version, executable_name):
    content = f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDisplayName</key>
    <string>{display_name}</string>

    <key>CFBundleName</key>
    <string>{bundle_name}</string>

    <key>CFBundleIdentifier</key>
    <string>{bundle_identifier}</string>

    <key>CFBundleVersion</key>
    <string>{bundle_version}</string>

    <key>CFBundleShortVersionString</key>
    <string>{bundle_version}</string>

    <key>CFBundleExecutable</key>
    <string>{executable_name}</string>

    <key>CFBundlePackageType</key>
    <string>APPL</string>

    <key>UIRequiredDeviceCapabilities</key>
    <array>
        <string>arm64</string>
    </array>

    <key>UILaunchStoryboardName</key>
    <string>LaunchScreen</string>

    <key>UIInterfaceOrientation</key>
    <string>UIInterfaceOrientationLandscapeLeft</string>

    <key>UISupportedInterfaceOrientations</key>
    <array>
        <string>UIInterfaceOrientationLandscapeLeft</string>
        <string>UIInterfaceOrientationLandscapeRight</string>
    </array>

    <key>UISupportedInterfaceOrientations~ipad</key>
    <array>
        <string>UIInterfaceOrientationLandscapeLeft</string>
        <string>UIInterfaceOrientationLandscapeRight</string>
    </array>

    <key>CFBundleIcons</key>
    <dict>
        <key>CFBundlePrimaryIcon</key>
        <dict>
            <key>CFBundleIconFiles</key>
            <array>
                <string>AppIcon</string>
            </array>
            <key>UIPrerenderedIcon</key>
            <false/>
        </dict>
    </dict>

    <key>SDLHintAccelerometerAsJoystick</key>
    <string>0</string>
</dict>
</plist>
"""
    with open("Info.plist", "w") as f:
        f.write(content)

if __name__ == "__main__":
    display_name = sys.argv[1]
    bundle_name = sys.argv[2]
    bundle_identifier = sys.argv[3]
    bundle_version = sys.argv[4]
    executable_name = sys.argv[5]

    generate_info_plist(display_name, bundle_name, bundle_identifier, bundle_version, executable_name)
