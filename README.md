Visual-Processing
=================

The visual processing perceptual components process a camera feed to provide metadata about the scene being depicted. SmartSearch provides three such components:

  * simple_camera: This demo component expects a USB camera connected to the computer and calculates the average frame intensity and the frame-by-frame difference. It serves as an example on how to interface the camera and how to populate feeds within C.
  * visual_scene_analysis: This is a complete visual perceptual component. It segments foreground objects (people, vehicles, etc.) in successive video frames and reports visual density, object count and persistent colours as a metadata streams.
  * face_tracking: This is an end-to-end face tracking system, employing CAM-Shift tracking and multiple target handling.
