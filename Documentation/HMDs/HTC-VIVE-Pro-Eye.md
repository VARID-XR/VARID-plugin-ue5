# HTC VIVE Pro Eye

## Specs
- AMOLED displays - 1440 x 1600 pixels per eye (2880 x 1600 pixels combined), 90 Hz, 110 degrees.
- https://www.vive.com/uk/product/vive-pro-eye/specs/
- In reality it is 110 degrees on vertical and 106 degrees on horizontal. source: https://forum.vive.com/topic/8550-configuration-of-fov-of-htc-vive-pro-eye/?ct=1626168483


## AR Plugin: SRWorks
- Stereo camera.
- This plugin enables realtime passthrough video from the VIVE Pro Eye front facing cameras into Unreal.
- Whilst the video is stereo and acceptable latency, the video is low relatively low resolution: 640x480.

- They can also capture up to 90fps with an average latency of 200ms, so they're good enough to avoid any lag-related nausea.


## Eye Tracking Plugin: SRanipal
- This 3rd party plugin provided by VIVE enables realtime eye tracking from VIVE Pro Eye Tobii.
- FIXED SRanipal : https://github.com/Temaran/SRanipalUE4SDK
- main tobii core API : https://vr.tobii.com/sdk/develop/ue4/api/core/

- Install runtime (VIVE_SRanipalInstaller_1.3.2.0.msi)
- It will probably fail
- Manually update sranipal service ini
	- C:\Users\joejb\AppData\LocalLow\HTC Corporation\SR_Config
	- EnableEyeTracking=1
	- AcceptEULA=1
- Copy unreal plugin to project plugins folder
- Restart unreal

System tray > Runtime > About: should report:
- runtime version: 1.3.2.0
- eye camera version: 2.41.0-942e3e4

- VERY IMPORTANT - once the SRanipal plugin has been enabled go to:
  -  Project settings > PLugins > SRanipal > Eye Settings > Enable Eye By default = ticked
  -  Project settings > PLugins > SRanipal > Eye Settings > Eye Version = 2