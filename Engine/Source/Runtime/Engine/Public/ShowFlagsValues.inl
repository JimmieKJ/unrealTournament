// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

// usage
//
// General purpose showflag (always variable):
// SHOWFLAG_ALWAYS_ACCESSIBLE( <showflag name>, <showflag group>, <Localized TEXT stuff>)
// Fixed in shipping builds:
// SHOWFLAG_FIXED_IN_SHIPPING( <showflag name>, <fixed bool>, <showflag group>, <Localized TEXT stuff>)

#ifndef SHOWFLAG_ALWAYS_ACCESSIBLE
#error SHOWFLAG_ALWAYS_ACCESSIBLE macro is undefined.
#endif

// the default case for SHOWFLAG_FIXED_IN_SHIPPING is to give flag name.
#ifndef SHOWFLAG_FIXED_IN_SHIPPING
#define SHOWFLAG_FIXED_IN_SHIPPING(a,...) SHOWFLAG_ALWAYS_ACCESSIBLE(a,__VA_ARGS__)
#endif

/** Affects all postprocessing features, depending on viewmode this is on or off */
SHOWFLAG_ALWAYS_ACCESSIBLE(PostProcessing, SFG_Hidden, LOCTEXT("PostProcessingSF", "Post-processing"))
/** Bloom */
SHOWFLAG_ALWAYS_ACCESSIBLE(Bloom, SFG_PostProcess, LOCTEXT("BloomSF", "Bloom"))
/** HDR->LDR conversion is done through a tone mapper (otherwise linear mapping is used) */
SHOWFLAG_ALWAYS_ACCESSIBLE(Tonemapper, SFG_PostProcess, LOCTEXT("TonemapperSF", "Tonemapper"))
/** Any Anti-aliasing e.g. FXAA, Temporal AA */
SHOWFLAG_ALWAYS_ACCESSIBLE(AntiAliasing, SFG_Normal, LOCTEXT("AntiAliasingSF", "Anti-aliasing"))
/** Only used in AntiAliasing is on, true:uses Temporal AA, otherwise FXAA */
SHOWFLAG_ALWAYS_ACCESSIBLE(TemporalAA, SFG_Advanced, LOCTEXT("TemporalAASF", "Temporal AA (instead FXAA)"))
/** e.g. Ambient cube map  */
SHOWFLAG_ALWAYS_ACCESSIBLE(AmbientCubemap, SFG_LightingFeatures, LOCTEXT("AmbientCubemapSF", "Ambient Cubemap"))
/** Human like eye simulation to adapt to the brightness of the view */
SHOWFLAG_ALWAYS_ACCESSIBLE(EyeAdaptation, SFG_PostProcess, LOCTEXT("EyeAdaptationSF", "Eye Adaptation"))
/** Display a histogram of the scene HDR color */
SHOWFLAG_ALWAYS_ACCESSIBLE(VisualizeHDR, SFG_Visualize, LOCTEXT("VisualizeHDRSF", "HDR (Eye Adaptation)"))
/** Image based lens flares (Simulate artifact of reflections within a camera system) */
SHOWFLAG_ALWAYS_ACCESSIBLE(LensFlares, SFG_PostProcess, LOCTEXT("LensFlaresSF", "Lens Flares"))
/** show indirect lighting component */
SHOWFLAG_ALWAYS_ACCESSIBLE(GlobalIllumination, SFG_LightingComponents, LOCTEXT("GlobalIlluminationSF", "Global Illumination"))
/** Darkens the screen borders (Camera artifact and artistic effect) */
SHOWFLAG_ALWAYS_ACCESSIBLE(Vignette, SFG_PostProcess, LOCTEXT("VignetteSF", "Vignette"))
/** Fine film grain */
SHOWFLAG_ALWAYS_ACCESSIBLE(Grain, SFG_PostProcess, LOCTEXT("GrainSF", "Grain"))
/** Screen Space Ambient Occlusion */
SHOWFLAG_ALWAYS_ACCESSIBLE(AmbientOcclusion, SFG_LightingComponents, LOCTEXT("AmbientOcclusionSF", "Ambient Occlusion"))
/** Decal rendering */
SHOWFLAG_ALWAYS_ACCESSIBLE(Decals, SFG_Normal, LOCTEXT("DecalsSF", "Decals"))
/** like bloom dirt mask */
SHOWFLAG_ALWAYS_ACCESSIBLE(CameraImperfections, SFG_PostProcess, LOCTEXT("CameraImperfectionsSF", "Camera Imperfections"))
/** to allow to disable visualizetexture for some editor rendering (e.g. thumbnail rendering) */
SHOWFLAG_ALWAYS_ACCESSIBLE(OnScreenDebug, SFG_Developer, LOCTEXT("OnScreenDebugSF", "On Screen Debug"))
/** needed for VMI_Lit_DetailLighting, Whether to override material diffuse and specular with constants, used by the Detail Lighting viewmode. */
SHOWFLAG_ALWAYS_ACCESSIBLE(OverrideDiffuseAndSpecular, SFG_Hidden, LOCTEXT("OverrideDiffuseAndSpecularSF", "Override Diffuse And Specular"))
/** needed for VMI_ReflectionOverride, Whether to override all materials to be smooth, mirror reflections. */
SHOWFLAG_ALWAYS_ACCESSIBLE(ReflectionOverride, SFG_Hidden, LOCTEXT("ReflectionOverrideSF", "Reflections"))
/** needed for VMI_VisualizeBuffer, Whether to enable the buffer visualization mode. */
SHOWFLAG_ALWAYS_ACCESSIBLE(VisualizeBuffer, SFG_Hidden, LOCTEXT("VisualizeBufferSF", "Buffer Visualization"))
/** Allows to disable all direct lighting (does not affect indirect light) */
SHOWFLAG_FIXED_IN_SHIPPING(DirectLighting, 1, SFG_LightingComponents, LOCTEXT("DirectLightingSF", "Direct Lighting"))
/** Allows to disable lighting from Directional Lights */
SHOWFLAG_FIXED_IN_SHIPPING(DirectionalLights, 1, SFG_LightingComponents, LOCTEXT("DirectionalLightsSF", "Directional Lights"))
/** Allows to disable lighting from Point Lights */
SHOWFLAG_FIXED_IN_SHIPPING(PointLights, 1, SFG_LightingComponents, LOCTEXT("PointLightsSF", "Point Lights"))
/** Allows to disable lighting from Spot Lights */
SHOWFLAG_FIXED_IN_SHIPPING(SpotLights, 1, SFG_LightingComponents, LOCTEXT("SpotLightsSF", "Spot Lights"))
/** Color correction after tone mapping */
SHOWFLAG_ALWAYS_ACCESSIBLE(ColorGrading, SFG_PostProcess, LOCTEXT("ColorGradingSF", "Color Grading"))
/** Visualize vector fields. */
SHOWFLAG_ALWAYS_ACCESSIBLE(VectorFields, SFG_Developer, LOCTEXT("VectorFieldsSF", "Vector Fields"))
/** Depth of Field */
SHOWFLAG_ALWAYS_ACCESSIBLE(DepthOfField, SFG_PostProcess, LOCTEXT("DepthOfFieldSF", "Depth Of Field"))
/** Whether editor-hidden primitives cast shadows. */
SHOWFLAG_ALWAYS_ACCESSIBLE(ShadowsFromEditorHiddenActors, SFG_Developer, LOCTEXT("ShadowsFromEditorHiddenActorsSF", "Shadows of Editor-Hidden Actors"))
/** Highlight materials that indicate performance issues or show unrealistic materials */
SHOWFLAG_ALWAYS_ACCESSIBLE(GBufferHints, SFG_Developer, LOCTEXT("GBufferHintsSF", "GBuffer Hints (material attributes)"))
/** MotionBlur, for now only camera motion blur */
SHOWFLAG_ALWAYS_ACCESSIBLE(MotionBlur, SFG_PostProcess, LOCTEXT("MotionBlurSF", "Motion Blur"))
/** Whether to render the editor gizmos and other foreground editor widgets off screen and apply them after post process */
SHOWFLAG_ALWAYS_ACCESSIBLE(CompositeEditorPrimitives, SFG_Developer, LOCTEXT("CompositeEditorPrimitivesSF", "Composite Editor Primitives"))
/** Shows a test image that allows to tweak the monitor colors, borders and allows to judge image and temporal aliasing  */
SHOWFLAG_ALWAYS_ACCESSIBLE(TestImage, SFG_Developer, LOCTEXT("TestImageSF", "Test Image"))
/** Helper to tweak depth of field */
SHOWFLAG_ALWAYS_ACCESSIBLE(VisualizeDOF, SFG_Visualize, LOCTEXT("VisualizeDOFSF", "Depth of Field Layers"))
/** Helper to tweak depth of field */
SHOWFLAG_ALWAYS_ACCESSIBLE(VisualizeAdaptiveDOF, SFG_Visualize, LOCTEXT("VisualizeAdaptiveDOFSF", "Adaptive Depth of Field"))
/** Show Vertex Colors */
SHOWFLAG_ALWAYS_ACCESSIBLE(VertexColors, SFG_Advanced, LOCTEXT("VertexColorsSF", "Vertex Colors"))
/** Render Post process (screen space) distortion/refraction */
SHOWFLAG_ALWAYS_ACCESSIBLE(Refraction, SFG_Developer, LOCTEXT("RefractionSF", "Refraction"))
/** Usually set in game or when previewing Matinee but not in editor, used for motion blur or any kind of rendering features that rely on the former frame */
SHOWFLAG_ALWAYS_ACCESSIBLE(CameraInterpolation, SFG_Hidden, LOCTEXT("CameraInterpolationSF", "Camera Interpolation"))
/** Post processing color fringe (chromatic aberration) */
SHOWFLAG_ALWAYS_ACCESSIBLE(SceneColorFringe, SFG_PostProcess, LOCTEXT("SceneColorFringeSF", "Scene Color Fringe"))
/** If Translucency should be rendered into a separate RT and composited without DepthOfField, can be disabled in the materials (affects sorting) */
SHOWFLAG_ALWAYS_ACCESSIBLE(SeparateTranslucency, SFG_Advanced, LOCTEXT("SeparateTranslucencySF", "Separate Translucency"))
/** If Screen Percentage should be applied (upscaling), useful to disable it in editor */
SHOWFLAG_ALWAYS_ACCESSIBLE(ScreenPercentage, SFG_PostProcess, LOCTEXT("ScreenPercentageSF", "Screen Percentage"))
/** Helper to tweak motion blur settings */
SHOWFLAG_ALWAYS_ACCESSIBLE(VisualizeMotionBlur, SFG_Visualize, LOCTEXT("VisualizeMotionBlurSF", "Motion Blur"))
/** Whether to display the Reflection Environment feature, which has local reflections from Reflection Capture actors. */
SHOWFLAG_ALWAYS_ACCESSIBLE(ReflectionEnvironment, SFG_LightingFeatures, LOCTEXT("ReflectionEnvironmentSF", "Reflection Environment"))
/** Visualize pixels that are outside of their object's bounding box (content error). */
SHOWFLAG_ALWAYS_ACCESSIBLE(VisualizeOutOfBoundsPixels, SFG_Visualize, LOCTEXT("VisualizeOutOfBoundsPixelsSF", "Out of Bounds Pixels"))
/** Whether to display the scene's diffuse. */
SHOWFLAG_ALWAYS_ACCESSIBLE(Diffuse, SFG_LightingComponents, LOCTEXT("DiffuseSF", "Diffuse"))
/** Whether to display the scene's specular, including reflections. */
SHOWFLAG_ALWAYS_ACCESSIBLE(Specular, SFG_LightingComponents, LOCTEXT("SpecularSF", "Specular"))
/** Outline around selected objects in the editor */
SHOWFLAG_ALWAYS_ACCESSIBLE(SelectionOutline, SFG_Hidden, LOCTEXT("SelectionOutlineSF", "Selection Outline"))
/** If screen space reflections are enabled */
SHOWFLAG_ALWAYS_ACCESSIBLE(ScreenSpaceReflections, SFG_LightingFeatures, LOCTEXT("ScreenSpaceReflectionsSF", "Screen Space Reflections"))
/** If Screen Space Subsurface Scattering enabled */
SHOWFLAG_FIXED_IN_SHIPPING(SubsurfaceScattering, 1, SFG_LightingFeatures, LOCTEXT("SubsurfaceScatteringSF", "Subsurface Scattering (Screen Space)"))
/** If Screen Space Subsurface Scattering visualization is enabled */
SHOWFLAG_FIXED_IN_SHIPPING(VisualizeSSS, 0, SFG_Visualize, LOCTEXT("VisualizeSSSSF", "Subsurface Scattering (Screen Space)"))
/** If the indirect lighting cache is enabled */
SHOWFLAG_ALWAYS_ACCESSIBLE(IndirectLightingCache, SFG_LightingFeatures, LOCTEXT("IndirectLightingCacheSF", "Indirect Lighting Cache"))
/** calls debug drawing for AIs */
SHOWFLAG_ALWAYS_ACCESSIBLE(DebugAI, SFG_Developer, LOCTEXT("DebugAISF", "AI Debug"))
/** calls debug drawing for whatever LogVisualizer wants to draw */
SHOWFLAG_ALWAYS_ACCESSIBLE(VisLog, SFG_Developer, LOCTEXT("VisLogSF", "Log Visualizer"))
/** whether to draw navigation data */
SHOWFLAG_ALWAYS_ACCESSIBLE(Navigation, SFG_Normal, LOCTEXT("NavigationSF", "Navigation"))
/** used by gameplay debugging components to debug-draw on screen */
SHOWFLAG_ALWAYS_ACCESSIBLE(GameplayDebug, SFG_Developer, LOCTEXT("GameplayDebugSF", "Gameplay Debug"))
/** LightProfiles, usually 1d textures to have a light  (IES) */
SHOWFLAG_ALWAYS_ACCESSIBLE(TexturedLightProfiles, SFG_LightingFeatures, LOCTEXT("TexturedLightProfilesSF", "Textured Light Profiles (IES Texture)"))
/** LightFunctions (masking light sources with a material) */
SHOWFLAG_ALWAYS_ACCESSIBLE(LightFunctions, SFG_LightingFeatures, LOCTEXT("LightFunctionsSF", "Light Functions"))
/** Hardware Tessellation (DX11 feature) */
SHOWFLAG_ALWAYS_ACCESSIBLE(Tessellation, SFG_Advanced, LOCTEXT("TessellationSF", "Tessellation"))
/** Draws instanced static meshes that are not foliage or grass. */
SHOWFLAG_ALWAYS_ACCESSIBLE(InstancedStaticMeshes, SFG_Advanced, LOCTEXT("InstancedStaticMeshesSF", "Instanced Static Meshes"))
/** Draws instanced foliage. */
SHOWFLAG_ALWAYS_ACCESSIBLE(InstancedFoliage, SFG_Advanced, LOCTEXT("InstancedFoliageSF", "Foliage"))
/** Draws instanced grass. */
SHOWFLAG_ALWAYS_ACCESSIBLE(InstancedGrass, SFG_Advanced, LOCTEXT("InstancedGrassSF", "Grass"))
/** non baked shadows */
SHOWFLAG_ALWAYS_ACCESSIBLE(DynamicShadows, SFG_LightingComponents, LOCTEXT("DynamicShadowsSF", "Dynamic Shadows"))
/** Particles */
SHOWFLAG_ALWAYS_ACCESSIBLE(Particles, SFG_Normal, LOCTEXT("ParticlesSF", "Particles Sprite"))
/** if SkeletalMeshes are getting rendered */
SHOWFLAG_ALWAYS_ACCESSIBLE(SkeletalMeshes, SFG_Normal, LOCTEXT("SkeletalMeshesSF", "Skeletal Meshes"))
/** if the builder brush (editor) is getting rendered */
SHOWFLAG_ALWAYS_ACCESSIBLE(BuilderBrush, SFG_Hidden, LOCTEXT("BuilderBrushSF", "Builder Brush"))
/** Render translucency */
SHOWFLAG_ALWAYS_ACCESSIBLE(Translucency, SFG_Normal, LOCTEXT("TranslucencySF", "Translucency"))
/** Draw billboard components */
SHOWFLAG_ALWAYS_ACCESSIBLE(BillboardSprites, SFG_Advanced, LOCTEXT("BillboardSpritesSF", "Billboard Sprites"))
/** Use LOD parenting, MinDrawDistance, etc. If disabled, will show LOD parenting lines */
SHOWFLAG_ALWAYS_ACCESSIBLE(LOD, SFG_Advanced, LOCTEXT("LODSF", "LOD Parenting"))
/** needed for VMI_LightComplexity */
SHOWFLAG_ALWAYS_ACCESSIBLE(LightComplexity, SFG_Hidden, LOCTEXT("LightComplexitySF", "Light Complexity"))
/** needed for VMI_ShaderComplexity, render world colored by shader complexity */
SHOWFLAG_ALWAYS_ACCESSIBLE(ShaderComplexity, SFG_Hidden, LOCTEXT("ShaderComplexitySF", "Shader Complexity"))
/** needed for VMI_StationaryLightOverlap, render world colored by stationary light overlap */
SHOWFLAG_ALWAYS_ACCESSIBLE(StationaryLightOverlap, SFG_Hidden, LOCTEXT("StationaryLightOverlapSF", "Stationary Light Overlap"))
/** needed for VMI_LightmapDensity and VMI_LitLightmapDensity, render checkerboard material with UVs scaled by lightmap resolution w. color tint for world-space lightmap density */
SHOWFLAG_ALWAYS_ACCESSIBLE(LightMapDensity, SFG_Hidden, LOCTEXT("LightMapDensitySF", "Light Map Density"))
/** Render streaming bounding volumes for the currently selected texture */
SHOWFLAG_ALWAYS_ACCESSIBLE(StreamingBounds, SFG_Advanced, LOCTEXT("StreamingBoundsSF", "Streaming Bounds"))
/** Render joint limits */
SHOWFLAG_FIXED_IN_SHIPPING(Constraints, 0, SFG_Advanced, LOCTEXT("ConstraintsSF", "Constraints"))
/** Draws camera frustums */
SHOWFLAG_ALWAYS_ACCESSIBLE(CameraFrustums, SFG_Advanced, LOCTEXT("CameraFrustumsSF", "Camera Frustums"))
/** Draw sound actor radii */
SHOWFLAG_ALWAYS_ACCESSIBLE(AudioRadius, SFG_Advanced, LOCTEXT("AudioRadiusSF", "Audio Radius"))
/** Colors BSP based on model component association */
SHOWFLAG_ALWAYS_ACCESSIBLE(BSPSplit, SFG_Advanced, LOCTEXT("BSPSplitSF", "BSP Split"))
/** show editor (wireframe) brushes */
SHOWFLAG_ALWAYS_ACCESSIBLE(Brushes, SFG_Hidden, LOCTEXT("BrushesSF", "Brushes"))
/** Show the usual material light interaction */
SHOWFLAG_ALWAYS_ACCESSIBLE(Lighting, SFG_Hidden, LOCTEXT("LightingSF", "Lighting"))
/** Execute the deferred light passes, can be disabled for debugging */
SHOWFLAG_ALWAYS_ACCESSIBLE(DeferredLighting, SFG_Advanced, LOCTEXT("DeferredLightingSF", "DeferredLighting"))
/** Special: Allows to hide objects in the editor, is evaluated per primitive */
SHOWFLAG_ALWAYS_ACCESSIBLE(Editor, SFG_Hidden, LOCTEXT("EditorSF", "Editor"))
/** needed for VMI_BrushWireframe and VMI_LitLightmapDensity, Draws BSP triangles */
SHOWFLAG_ALWAYS_ACCESSIBLE(BSPTriangles, SFG_Hidden, LOCTEXT("BSPTrianglesSF", "BSP Triangles"))
/** Displays large clickable icons on static mesh vertices */
SHOWFLAG_ALWAYS_ACCESSIBLE(LargeVertices, SFG_Advanced, LOCTEXT("LargeVerticesSF", "Large Vertices"))
/** To show the grid in editor (grey lines and red dots) */
SHOWFLAG_ALWAYS_ACCESSIBLE(Grid, SFG_Normal, LOCTEXT("GridSF", "Grid"))
/** To show the snap in editor (only for editor view ports, red dots) */
SHOWFLAG_ALWAYS_ACCESSIBLE(Snap, SFG_Hidden, LOCTEXT("SnapSF", "Snap"))
/** In the filled view modeModeWidgetss, render mesh edges as well as the filled surfaces. */
SHOWFLAG_ALWAYS_ACCESSIBLE(MeshEdges, SFG_Advanced, LOCTEXT("MeshEdgesSF", "Mesh Edges"))
/** Complex cover rendering */
SHOWFLAG_ALWAYS_ACCESSIBLE(Cover, SFG_Hidden, LOCTEXT("CoverSF", "Cover"))
/** Spline rendering */
SHOWFLAG_ALWAYS_ACCESSIBLE(Splines, SFG_Advanced, LOCTEXT("SplinesSF", "Splines"))
/** Selection rendering, could be useful in game as well */
SHOWFLAG_ALWAYS_ACCESSIBLE(Selection, SFG_Advanced, LOCTEXT("SelectionSF", "Selection"))
/** Draws mode specific widgets and controls in the viewports (should only be set on viewport clients that are editing the level itself) */
SHOWFLAG_ALWAYS_ACCESSIBLE(ModeWidgets, SFG_Advanced, LOCTEXT("ModeWidgetsSF", "Mode Widgets"))
/**  */
SHOWFLAG_ALWAYS_ACCESSIBLE(Bounds, SFG_Advanced, LOCTEXT("BoundsSF", "Bounds"))
/** Draws each hit proxy in the scene with a different color */
SHOWFLAG_ALWAYS_ACCESSIBLE(HitProxies, SFG_Developer, LOCTEXT("HitProxiesSF", "Hit Proxies"))
/** Render objects with colors based on the property values */
SHOWFLAG_ALWAYS_ACCESSIBLE(PropertyColoration, SFG_Advanced, LOCTEXT("PropertyColorationSF", "Property Coloration"))
/** Draw lines to lights affecting this mesh if its selected. */
SHOWFLAG_ALWAYS_ACCESSIBLE(LightInfluences, SFG_Advanced, LOCTEXT("LightInfluencesSF", "Light Influences"))
/**  */
SHOWFLAG_ALWAYS_ACCESSIBLE(Pivot, SFG_Hidden, LOCTEXT("PivotSF", "Pivot"))
/** Draws un-occluded shadow frustums in wireframe */
SHOWFLAG_ALWAYS_ACCESSIBLE(ShadowFrustums, SFG_Advanced, LOCTEXT("ShadowFrustumsSF", "Shadow Frustums"))
/** needed for VMI_Wireframe and VMI_BrushWireframe */
SHOWFLAG_ALWAYS_ACCESSIBLE(Wireframe, SFG_Hidden, LOCTEXT("WireframeSF", "Wireframe"))
/**  */
SHOWFLAG_ALWAYS_ACCESSIBLE(Materials, SFG_Hidden, LOCTEXT("MaterialsSF", "Materials"))
/**  */
SHOWFLAG_ALWAYS_ACCESSIBLE(StaticMeshes, SFG_Normal, LOCTEXT("StaticMeshesSF", "Static Meshes"))
/**  */
SHOWFLAG_ALWAYS_ACCESSIBLE(Landscape, SFG_Normal, LOCTEXT("LandscapeSF", "Landscape"))
/**  */
SHOWFLAG_ALWAYS_ACCESSIBLE(LightRadius, SFG_Advanced, LOCTEXT("LightRadiusSF", "Light Radius"))
/** Draws fog */
SHOWFLAG_ALWAYS_ACCESSIBLE(Fog, SFG_Normal, LOCTEXT("FogSF", "Fog"))
/** Draws Volumes */
SHOWFLAG_ALWAYS_ACCESSIBLE(Volumes, SFG_Advanced, LOCTEXT("VolumesSF", "Volumes"))
/** if this is a game viewport, needed? */
SHOWFLAG_ALWAYS_ACCESSIBLE(Game, SFG_Hidden, LOCTEXT("GameSF", "Game"))
/** Render objects with colors based on what the level they belong to */
SHOWFLAG_ALWAYS_ACCESSIBLE(LevelColoration, SFG_Advanced, LOCTEXT("LevelColorationSF", "Level Coloration"))
/** Draws BSP brushes (in game or editor textured triangles usually with lightmaps) */
SHOWFLAG_ALWAYS_ACCESSIBLE(BSP, SFG_Normal, LOCTEXT("BSPSF", "BSP"))
/** Collision drawing */
SHOWFLAG_ALWAYS_ACCESSIBLE(Collision, SFG_Normal, LOCTEXT("CollisionWireFrame", "Collision"))
/** Collision blocking visibility against complex **/
SHOWFLAG_ALWAYS_ACCESSIBLE(CollisionVisibility, SFG_Hidden, LOCTEXT("CollisionVisibility", "Visibility"))
/** Collision blocking pawn against simple collision **/
SHOWFLAG_ALWAYS_ACCESSIBLE(CollisionPawn, SFG_Hidden, LOCTEXT("CollisionPawn", "Pawn"))
/** Render LightShafts */
SHOWFLAG_ALWAYS_ACCESSIBLE(LightShafts, SFG_LightingFeatures, LOCTEXT("LightShaftsSF", "Light Shafts"))
/** Render the PostProcess Material */
SHOWFLAG_ALWAYS_ACCESSIBLE(PostProcessMaterial, SFG_PostProcess, LOCTEXT("PostProcessMaterialSF", "Post Process Material"))
/** Render Atmospheric scattering (Atmospheric Fog) */
SHOWFLAG_ALWAYS_ACCESSIBLE(AtmosphericFog, SFG_Advanced, LOCTEXT("AtmosphereSF", "Atmospheric Fog"))
/** Render safe frames bars*/
SHOWFLAG_ALWAYS_ACCESSIBLE(CameraAspectRatioBars, SFG_Advanced, LOCTEXT("CameraAspectRatioBarsSF", "Camera Aspect Ratio Bars"))
/** Render safe frames */
SHOWFLAG_ALWAYS_ACCESSIBLE(CameraSafeFrames, SFG_Advanced, LOCTEXT("CameraSafeFramesSF", "Camera Safe Frames"))
/** Render TextRenderComponents (3D text) */
SHOWFLAG_ALWAYS_ACCESSIBLE(TextRender, SFG_Advanced, LOCTEXT("TextRenderSF", "Render (3D) Text"))
/** Any rendering/buffer clearing  (good for benchmarking) */
SHOWFLAG_ALWAYS_ACCESSIBLE(Rendering, SFG_Hidden, LOCTEXT("RenderingSF", "Any Rendering"))
/** Show the current mask being used by the highres screenshot capture */
SHOWFLAG_ALWAYS_ACCESSIBLE(HighResScreenshotMask, SFG_Hidden, LOCTEXT("HighResScreenshotMaskSF", "High Res Screenshot Mask"))
/** Distortion of output for HMD devices */
SHOWFLAG_ALWAYS_ACCESSIBLE(HMDDistortion, SFG_PostProcess, LOCTEXT("HMDDistortionSF", "HMD Distortion"))
/** Whether to render in stereoscopic 3d */
SHOWFLAG_ALWAYS_ACCESSIBLE(StereoRendering, SFG_Hidden, LOCTEXT("StereoRenderingSF", "Stereoscopic Rendering"))
/** Show objects even if they should be distance culled */
SHOWFLAG_ALWAYS_ACCESSIBLE(DistanceCulledPrimitives, SFG_Hidden, LOCTEXT("DistanceCulledPrimitivesSF", "Distance Culled Primitives"))
/** To visualize the culling in Tile Based Deferred Lighting, later for non tiled as well */
SHOWFLAG_ALWAYS_ACCESSIBLE(VisualizeLightCulling, SFG_Hidden, LOCTEXT("VisualizeLightCullingSF", "Light Culling"))
/** To disable precomputed visibility */
SHOWFLAG_ALWAYS_ACCESSIBLE(PrecomputedVisibility, SFG_Advanced, LOCTEXT("PrecomputedVisibilitySF", "Precomputed Visibility"))
/** Contribution from sky light */
SHOWFLAG_ALWAYS_ACCESSIBLE(SkyLighting, SFG_LightingComponents, LOCTEXT("SkyLightingSF", "Sky Lighting"))
/** Visualize Light Propagation Volume, for developer (by default off): */
SHOWFLAG_ALWAYS_ACCESSIBLE(VisualizeLPV, SFG_Visualize, LOCTEXT("VisualizeLPVSF", "Light Propagation Volume"))
/** Visualize preview shadow indicator */
SHOWFLAG_ALWAYS_ACCESSIBLE(PreviewShadowsIndicator, SFG_Visualize, LOCTEXT("PreviewShadowIndicatorSF", "Preview Shadows Indicator"))
/** Visualize precomputed visibilty cells */
SHOWFLAG_ALWAYS_ACCESSIBLE(PrecomputedVisibilityCells, SFG_Visualize, LOCTEXT("PrecomputedVisibilityCellsSF", "Precomputed Visibility Cells"))
/** Visualize volume lighting samples used for GI on dynamic objects */
SHOWFLAG_ALWAYS_ACCESSIBLE(VolumeLightingSamples, SFG_Visualize, LOCTEXT("VolumeLightingSamplesSF", "Volume Lighting Samples"))
/** needed for VMI_LpvLightingViewMode, Whether to show only LPV lighting*/
SHOWFLAG_ALWAYS_ACCESSIBLE(LpvLightingOnly, SFG_Hidden, LOCTEXT("VisualizeLPVSF_ViewMode", "Visualize LPV"))
/** Render Paper2D sprites */
SHOWFLAG_ALWAYS_ACCESSIBLE(Paper2DSprites, SFG_Advanced, LOCTEXT("Paper2DSpritesSF", "Paper 2D Sprites"))
/** Visualization of distance field AO */
SHOWFLAG_ALWAYS_ACCESSIBLE(VisualizeDistanceFieldAO, SFG_Visualize, LOCTEXT("VisualizeDistanceFieldAOSF", "Distance Field Ambient Occlusion"))
/** Visualization of distance field GI */
SHOWFLAG_ALWAYS_ACCESSIBLE(VisualizeDistanceFieldGI, SFG_Visualize, LOCTEXT("VisualizeDistanceFieldGISF", "Distance Field Global Illumination"))
/** Mesh Distance fields */
SHOWFLAG_ALWAYS_ACCESSIBLE(VisualizeMeshDistanceFields, SFG_Visualize, LOCTEXT("MeshDistanceFieldsSF", "Mesh DistanceFields"))
/** Distance field AO */
SHOWFLAG_ALWAYS_ACCESSIBLE(DistanceFieldAO, SFG_LightingFeatures, LOCTEXT("DistanceFieldAOSF", "Distance Field Ambient Occlusion"))
/** Distance field GI */
SHOWFLAG_ALWAYS_ACCESSIBLE(DistanceFieldGI, SFG_LightingFeatures, LOCTEXT("DistanceFieldGISF", "Distance Field Global Illumination"))
/** Visualize screen space reflections, for developer (by default off): */
SHOWFLAG_FIXED_IN_SHIPPING(VisualizeSSR, 0, SFG_Visualize, LOCTEXT("VisualizeSSR", "Screen Space Reflections"))
/** Force the use of the GBuffer. */
SHOWFLAG_ALWAYS_ACCESSIBLE(ForceGBuffer, SFG_Hidden, LOCTEXT("ForceGBuffer", "Force usage of GBuffer"))
/** Visualize the senses configuration of AIs' PawnSensingComponent */
SHOWFLAG_ALWAYS_ACCESSIBLE(VisualizeSenses, SFG_Advanced, LOCTEXT("VisualizeSenses", "Senses"))
/** Visualize the bloom, for developer (by default off): */
SHOWFLAG_FIXED_IN_SHIPPING(VisualizeBloom, 0, SFG_Visualize, LOCTEXT("VisualizeBloom", "Bloom"))

#undef SHOWFLAG_ALWAYS_ACCESSIBLE
#undef SHOWFLAG_FIXED_IN_SHIPPING
