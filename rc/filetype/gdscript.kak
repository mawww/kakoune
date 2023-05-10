# http://godotengine.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](gd) %{
    set-option buffer filetype gdscript
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=gdscript %{
    require-module gdscript

    set-option window static_words %opt{gdscript_static_words}

    hook window InsertChar \n -group gdscript-insert gdscript-insert-on-new-line
    hook window InsertChar \n -group gdscript-indent gdscript-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange pop:insert:.* -group gdscript-trim-indent %{ try %{ execute-keys -draft <semicolon> x s ^\h+$ <ret> d } }
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window gdscript-.+ }
}

hook -group gdscript-highlight global WinSetOption filetype=gdscript %{
    add-highlighter window/gdscript ref gdscript
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/gdscript }
}

provide-module gdscript %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/gdscript regions
add-highlighter shared/gdscript/code default-region group

add-highlighter shared/gdscript/string region -match-capture ("|'|"""|''') (?<!\\)(?:\\\\)*("|'|"""|''') group
add-highlighter shared/gdscript/string/ fill string
add-highlighter shared/gdscript/string/ regex \\[abfnrtv\n\\]                            0:meta
add-highlighter shared/gdscript/string/ regex '%%'                                       0:meta
add-highlighter shared/gdscript/string/ regex '%[cs]'                                    0:value
add-highlighter shared/gdscript/string/ regex '%0?[+-]?([\d]*|\*?)\.?([\d]*|\*?)[dfoxX]' 0:value
add-highlighter shared/gdscript/string/ regex '%0?([\d]*|\*?)\.?([\d]*|\*?)[+-]?[dfoxX]' 0:value

add-highlighter shared/gdscript/comment region '#' $ fill comment

# integers
add-highlighter shared/gdscript/code/ regex (?i)\b0b[01]+l?\b                    0:value
add-highlighter shared/gdscript/code/ regex (?i)\b0x[\da-f]+l?\b                 0:value
add-highlighter shared/gdscript/code/ regex (?i)\b0o?[0-7]+l?\b                  0:value
add-highlighter shared/gdscript/code/ regex (?i)\b([1-9]\d*|0)l?\b               0:value
# floats
add-highlighter shared/gdscript/code/ regex \b\d+[eE][+-]?\d+\b                  0:value
add-highlighter shared/gdscript/code/ regex (\b\d+)?\.\d+\b                      0:value
add-highlighter shared/gdscript/code/ regex \b\d+\.                              0:value
# functions
add-highlighter shared/gdscript/code/ regex _?[a-zA-Z]\w*\s*(?=\()               0:function
add-highlighter shared/gdscript/code/ regex (?:func\h+)(_?\w+)(?:<[^>]+?>)?\(    1:function
# operators
add-highlighter shared/gdscript/code/ regex \+|-|\*|/|%|=|<|>|&|\||\^|~|:=       0:operator
# constants & enums
add-highlighter shared/gdscript/code/ regex \b[A-Z_][A-Z0-9_]*\b                 0:variable
# annotations
add-highlighter shared/gdscript/code/ regex @\w+                                 0:attribute
# special case of get =, set =
add-highlighter shared/gdscript/code/ regex \b(get)\h*=\h*(\w+)                  1:keyword 2:function
add-highlighter shared/gdscript/code/ regex \b(set)\h*=\h*(\w+)                  1:keyword 2:function
# keywords and built-ins
evaluate-commands %sh{
    keywords="as await break breakpoint class class_name const continue elif else enum extends for func if in is match pass return self signal static super var void while"

    values="false true null"

    # Built in `nushell` with `ls doc/classes | each {open $in.name | get attributes.name} | str join " "`, the rest are built similarly
    builtin_classes="AABB AESContext AStar2D AStar3D AStarGrid2D AcceptDialog AnimatableBody2D AnimatableBody3D AnimatedSprite2D AnimatedSprite3D AnimatedTexture Animation AnimationLibrary AnimationNode AnimationNodeAdd2 AnimationNodeAdd3 AnimationNodeAnimation AnimationNodeBlend2 AnimationNodeBlend3 AnimationNodeBlendSpace1D AnimationNodeBlendSpace2D AnimationNodeBlendTree AnimationNodeOneShot AnimationNodeOutput AnimationNodeStateMachine AnimationNodeStateMachinePlayback AnimationNodeStateMachineTransition AnimationNodeSync AnimationNodeTimeScale AnimationNodeTimeSeek AnimationNodeTransition AnimationPlayer AnimationRootNode AnimationTrackEditPlugin AnimationTree Area2D Area3D Array ArrayMesh ArrayOccluder3D AspectRatioContainer AtlasTexture AudioBusLayout AudioEffect AudioEffectAmplify AudioEffectBandLimitFilter AudioEffectBandPassFilter AudioEffectCapture AudioEffectChorus AudioEffectCompressor AudioEffectDelay AudioEffectDistortion AudioEffectEQ AudioEffectEQ10 AudioEffectEQ21 AudioEffectEQ6 AudioEffectFilter AudioEffectHighPassFilter AudioEffectHighShelfFilter AudioEffectInstance AudioEffectLimiter AudioEffectLowPassFilter AudioEffectLowShelfFilter AudioEffectNotchFilter AudioEffectPanner AudioEffectPhaser AudioEffectPitchShift AudioEffectRecord AudioEffectReverb AudioEffectSpectrumAnalyzer AudioEffectSpectrumAnalyzerInstance AudioEffectStereoEnhance AudioListener2D AudioListener3D AudioServer AudioStream AudioStreamGenerator AudioStreamGeneratorPlayback AudioStreamMicrophone AudioStreamPlayback AudioStreamPlaybackPolyphonic AudioStreamPlaybackResampled AudioStreamPlayer AudioStreamPlayer2D AudioStreamPlayer3D AudioStreamPolyphonic AudioStreamRandomizer AudioStreamWAV BackBufferCopy BaseButton BaseMaterial3D Basis BitMap Bone2D BoneAttachment3D BoneMap BoxContainer BoxMesh BoxOccluder3D BoxShape3D Button ButtonGroup CPUParticles2D CPUParticles3D Callable CallbackTweener Camera2D Camera3D CameraAttributes CameraAttributesPhysical CameraAttributesPractical CameraFeed CameraServer CameraTexture CanvasGroup CanvasItem CanvasItemMaterial CanvasLayer CanvasModulate CanvasTexture CapsuleMesh CapsuleShape2D CapsuleShape3D CenterContainer CharFXTransform CharacterBody2D CharacterBody3D CheckBox CheckButton CircleShape2D ClassDB CodeEdit CodeHighlighter CollisionObject2D CollisionObject3D CollisionPolygon2D CollisionPolygon3D CollisionShape2D CollisionShape3D Color ColorPicker ColorPickerButton ColorRect CompressedCubemap CompressedCubemapArray CompressedTexture2D CompressedTexture2DArray CompressedTexture3D CompressedTextureLayered ConcavePolygonShape2D ConcavePolygonShape3D ConeTwistJoint3D ConfigFile ConfirmationDialog Container Control ConvexPolygonShape2D ConvexPolygonShape3D Crypto CryptoKey Cubemap CubemapArray Curve Curve2D Curve3D CurveTexture CurveXYZTexture CylinderMesh CylinderShape3D DTLSServer DampedSpringJoint2D Decal Dictionary DirAccess DirectionalLight2D DirectionalLight3D DisplayServer EditorCommandPalette EditorDebuggerPlugin EditorDebuggerSession EditorExportPlatform EditorExportPlugin EditorFeatureProfile EditorFileDialog EditorFileSystem EditorFileSystemDirectory EditorFileSystemImportFormatSupportQuery EditorImportPlugin EditorInspector EditorInspectorPlugin EditorInterface EditorNode3DGizmo EditorNode3DGizmoPlugin EditorPaths EditorPlugin EditorProperty EditorResourceConversionPlugin EditorResourcePicker EditorResourcePreview EditorResourcePreviewGenerator EditorSceneFormatImporter EditorScenePostImport EditorScenePostImportPlugin EditorScript EditorScriptPicker EditorSelection EditorSettings EditorSpinSlider EditorSyntaxHighlighter EditorTranslationParserPlugin EditorUndoRedoManager EditorVCSInterface EncodedObjectAsID Engine EngineDebugger EngineProfiler Environment Expression FileAccess FileDialog FileSystemDock FlowContainer FogMaterial FogVolume Font FontFile FontVariation GDExtension GDExtensionManager GPUParticles2D GPUParticles3D GPUParticlesAttractor3D GPUParticlesAttractorBox3D GPUParticlesAttractorSphere3D GPUParticlesAttractorVectorField3D GPUParticlesCollision3D GPUParticlesCollisionBox3D GPUParticlesCollisionHeightField3D GPUParticlesCollisionSDF3D GPUParticlesCollisionSphere3D Generic6DOFJoint3D Geometry2D Geometry3D GeometryInstance3D Gradient GradientTexture1D GradientTexture2D GraphEdit GraphNode GridContainer GrooveJoint2D HBoxContainer HFlowContainer HMACContext HScrollBar HSeparator HSlider HSplitContainer HTTPClient HTTPRequest HashingContext HeightMapShape3D HingeJoint3D IP Image ImageFormatLoader ImageFormatLoaderExtension ImageTexture ImageTexture3D ImageTextureLayered ImmediateMesh ImporterMesh ImporterMeshInstance3D Input InputEvent InputEventAction InputEventFromWindow InputEventGesture InputEventJoypadButton InputEventJoypadMotion InputEventKey InputEventMIDI InputEventMagnifyGesture InputEventMouse InputEventMouseButton InputEventMouseMotion InputEventPanGesture InputEventScreenDrag InputEventScreenTouch InputEventShortcut InputEventWithModifiers InputMap InstancePlaceholder IntervalTweener ItemList JNISingleton JSON JSONRPC JavaClass JavaClassWrapper JavaScriptBridge JavaScriptObject Joint2D Joint3D KinematicCollision2D KinematicCollision3D Label Label3D LabelSettings Light2D Light3D LightOccluder2D LightmapGI LightmapGIData LightmapProbe Lightmapper LightmapperRD Line2D LineEdit LinkButton MainLoop MarginContainer Marker2D Marker3D Marshalls Material MenuBar MenuButton Mesh MeshDataTool MeshInstance2D MeshInstance3D MeshLibrary MeshTexture MethodTweener MissingNode MissingResource MovieWriter MultiMesh MultiMeshInstance2D MultiMeshInstance3D MultiplayerAPI MultiplayerAPIExtension MultiplayerPeer MultiplayerPeerExtension Mutex NavigationAgent2D NavigationAgent3D NavigationLink2D NavigationLink3D NavigationMesh NavigationMeshGenerator NavigationObstacle2D NavigationObstacle3D NavigationPathQueryParameters2D NavigationPathQueryParameters3D NavigationPathQueryResult2D NavigationPathQueryResult3D NavigationPolygon NavigationRegion2D NavigationRegion3D NavigationServer2D NavigationServer3D NinePatchRect Node Node2D Node3D Node3DGizmo NodePath ORMMaterial3D OS Object Occluder3D OccluderInstance3D OccluderPolygon2D OfflineMultiplayerPeer OmniLight3D OptimizedTranslation OptionButton PCKPacker PackedByteArray PackedColorArray PackedDataContainer PackedDataContainerRef PackedFloat32Array PackedFloat64Array PackedInt32Array PackedInt64Array PackedScene PackedStringArray PackedVector2Array PackedVector3Array PacketPeer PacketPeerDTLS PacketPeerExtension PacketPeerStream PacketPeerUDP Panel PanelContainer PanoramaSkyMaterial ParallaxBackground ParallaxLayer ParticleProcessMaterial Path2D Path3D PathFollow2D PathFollow3D Performance PhysicalBone2D PhysicalBone3D PhysicalSkyMaterial PhysicsBody2D PhysicsBody3D PhysicsDirectBodyState2D PhysicsDirectBodyState2DExtension PhysicsDirectBodyState3D PhysicsDirectBodyState3DExtension PhysicsDirectSpaceState2D PhysicsDirectSpaceState2DExtension PhysicsDirectSpaceState3D PhysicsDirectSpaceState3DExtension PhysicsMaterial PhysicsPointQueryParameters2D PhysicsPointQueryParameters3D PhysicsRayQueryParameters2D PhysicsRayQueryParameters3D PhysicsServer2D PhysicsServer2DExtension PhysicsServer2DManager PhysicsServer3D PhysicsServer3DExtension PhysicsServer3DManager PhysicsServer3DRenderingServerHandler PhysicsShapeQueryParameters2D PhysicsShapeQueryParameters3D PhysicsTestMotionParameters2D PhysicsTestMotionParameters3D PhysicsTestMotionResult2D PhysicsTestMotionResult3D PinJoint2D PinJoint3D PlaceholderCubemap PlaceholderCubemapArray PlaceholderMaterial PlaceholderMesh PlaceholderTexture2D PlaceholderTexture2DArray PlaceholderTexture3D PlaceholderTextureLayered Plane PlaneMesh PointLight2D PointMesh Polygon2D PolygonOccluder3D PolygonPathFinder Popup PopupMenu PopupPanel PortableCompressedTexture2D PrimitiveMesh PrismMesh ProceduralSkyMaterial ProgressBar ProjectSettings Projection PropertyTweener QuadMesh QuadOccluder3D Quaternion RDAttachmentFormat RDFramebufferPass RDPipelineColorBlendState RDPipelineColorBlendStateAttachment RDPipelineDepthStencilState RDPipelineMultisampleState RDPipelineRasterizationState RDPipelineSpecializationConstant RDSamplerState RDShaderFile RDShaderSPIRV RDShaderSource RDTextureFormat RDTextureView RDUniform RDVertexAttribute RID RandomNumberGenerator Range RayCast2D RayCast3D Rect2 Rect2i RectangleShape2D RefCounted ReferenceRect ReflectionProbe RemoteTransform2D RemoteTransform3D RenderingDevice RenderingServer Resource ResourceFormatLoader ResourceFormatSaver ResourceImporter ResourceLoader ResourcePreloader ResourceSaver ResourceUID RibbonTrailMesh RichTextEffect RichTextLabel RigidBody2D RigidBody3D RootMotionView SceneState SceneTree SceneTreeTimer Script ScriptCreateDialog ScriptEditor ScriptEditorBase ScriptExtension ScriptLanguage ScriptLanguageExtension ScrollBar ScrollContainer SegmentShape2D Semaphore SeparationRayShape2D SeparationRayShape3D Separator Shader ShaderGlobalsOverride ShaderInclude ShaderMaterial Shape2D Shape3D ShapeCast2D ShapeCast3D Shortcut Signal Skeleton2D Skeleton3D SkeletonIK3D SkeletonModification2D SkeletonModification2DCCDIK SkeletonModification2DFABRIK SkeletonModification2DJiggle SkeletonModification2DLookAt SkeletonModification2DPhysicalBones SkeletonModification2DStackHolder SkeletonModification2DTwoBoneIK SkeletonModificationStack2D SkeletonProfile SkeletonProfileHumanoid Skin SkinReference Sky Slider SliderJoint3D SoftBody3D SphereMesh SphereOccluder3D SphereShape3D SpinBox SplitContainer SpotLight3D SpringArm3D Sprite2D Sprite3D SpriteBase3D SpriteFrames StandardMaterial3D StaticBody2D StaticBody3D StreamPeer StreamPeerBuffer StreamPeerExtension StreamPeerGZIP StreamPeerTCP StreamPeerTLS String StringName StyleBox StyleBoxEmpty StyleBoxFlat StyleBoxLine StyleBoxTexture SubViewport SubViewportContainer SurfaceTool SyntaxHighlighter SystemFont TCPServer TLSOptions TabBar TabContainer TextEdit TextLine TextMesh TextParagraph TextServer TextServerDummy TextServerExtension TextServerManager Texture Texture2D Texture2DArray Texture3D TextureButton TextureLayered TextureProgressBar TextureRect Theme ThemeDB Thread TileData TileMap TileMapPattern TileSet TileSetAtlasSource TileSetScenesCollectionSource TileSetSource Time Timer TorusMesh TouchScreenButton Transform2D Transform3D Translation TranslationServer Tree TreeItem TriangleMesh TubeTrailMesh Tween Tweener UDPServer UndoRedo VBoxContainer VFlowContainer VScrollBar VSeparator VSlider VSplitContainer Variant Vector2 Vector2i Vector3 Vector3i Vector4 Vector4i VehicleBody3D VehicleWheel3D VideoStream VideoStreamPlayback VideoStreamPlayer Viewport ViewportTexture VisibleOnScreenEnabler2D VisibleOnScreenEnabler3D VisibleOnScreenNotifier2D VisibleOnScreenNotifier3D VisualInstance3D VisualShader VisualShaderNode VisualShaderNodeBillboard VisualShaderNodeBooleanConstant VisualShaderNodeBooleanParameter VisualShaderNodeClamp VisualShaderNodeColorConstant VisualShaderNodeColorFunc VisualShaderNodeColorOp VisualShaderNodeColorParameter VisualShaderNodeComment VisualShaderNodeCompare VisualShaderNodeConstant VisualShaderNodeCubemap VisualShaderNodeCubemapParameter VisualShaderNodeCurveTexture VisualShaderNodeCurveXYZTexture VisualShaderNodeCustom VisualShaderNodeDerivativeFunc VisualShaderNodeDeterminant VisualShaderNodeDistanceFade VisualShaderNodeDotProduct VisualShaderNodeExpression VisualShaderNodeFaceForward VisualShaderNodeFloatConstant VisualShaderNodeFloatFunc VisualShaderNodeFloatOp VisualShaderNodeFloatParameter VisualShaderNodeFresnel VisualShaderNodeGlobalExpression VisualShaderNodeGroupBase VisualShaderNodeIf VisualShaderNodeInput VisualShaderNodeIntConstant VisualShaderNodeIntFunc VisualShaderNodeIntOp VisualShaderNodeIntParameter VisualShaderNodeIs VisualShaderNodeLinearSceneDepth VisualShaderNodeMix VisualShaderNodeMultiplyAdd VisualShaderNodeOuterProduct VisualShaderNodeOutput VisualShaderNodeParameter VisualShaderNodeParameterRef VisualShaderNodeParticleAccelerator VisualShaderNodeParticleBoxEmitter VisualShaderNodeParticleConeVelocity VisualShaderNodeParticleEmit VisualShaderNodeParticleEmitter VisualShaderNodeParticleMeshEmitter VisualShaderNodeParticleMultiplyByAxisAngle VisualShaderNodeParticleOutput VisualShaderNodeParticleRandomness VisualShaderNodeParticleRingEmitter VisualShaderNodeParticleSphereEmitter VisualShaderNodeProximityFade VisualShaderNodeRandomRange VisualShaderNodeRemap VisualShaderNodeResizableBase VisualShaderNodeSDFRaymarch VisualShaderNodeSDFToScreenUV VisualShaderNodeSample3D VisualShaderNodeScreenUVToSDF VisualShaderNodeSmoothStep VisualShaderNodeStep VisualShaderNodeSwitch VisualShaderNodeTexture VisualShaderNodeTexture2DArray VisualShaderNodeTexture2DArrayParameter VisualShaderNodeTexture2DParameter VisualShaderNodeTexture3D VisualShaderNodeTexture3DParameter VisualShaderNodeTextureParameter VisualShaderNodeTextureParameterTriplanar VisualShaderNodeTextureSDF VisualShaderNodeTextureSDFNormal VisualShaderNodeTransformCompose VisualShaderNodeTransformConstant VisualShaderNodeTransformDecompose VisualShaderNodeTransformFunc VisualShaderNodeTransformOp VisualShaderNodeTransformParameter VisualShaderNodeTransformVecMult VisualShaderNodeUIntConstant VisualShaderNodeUIntFunc VisualShaderNodeUIntOp VisualShaderNodeUIntParameter VisualShaderNodeUVFunc VisualShaderNodeUVPolarCoord VisualShaderNodeVarying VisualShaderNodeVaryingGetter VisualShaderNodeVaryingSetter VisualShaderNodeVec2Constant VisualShaderNodeVec2Parameter VisualShaderNodeVec3Constant VisualShaderNodeVec3Parameter VisualShaderNodeVec4Constant VisualShaderNodeVec4Parameter VisualShaderNodeVectorBase VisualShaderNodeVectorCompose VisualShaderNodeVectorDecompose VisualShaderNodeVectorDistance VisualShaderNodeVectorFunc VisualShaderNodeVectorLen VisualShaderNodeVectorOp VisualShaderNodeVectorRefract VoxelGI VoxelGIData WeakRef Window WorkerThreadPool World2D World3D WorldBoundaryShape2D WorldBoundaryShape3D WorldEnvironment X509Certificate XMLParser XRAnchor3D XRCamera3D XRController3D XRInterface XRInterfaceExtension XRNode3D XROrigin3D XRPose XRPositionalTracker XRServer bool float int"
    
    builtin_methods="abs absf absi acos asin atan atan2 bezier_derivative bezier_interpolate bytes_to_var bytes_to_var_with_objects ceil ceilf ceili clamp clampf clampi cos cosh cubic_interpolate cubic_interpolate_angle cubic_interpolate_angle_in_time cubic_interpolate_in_time db_to_linear deg_to_rad ease error_string exp floor floorf floori fmod fposmod hash instance_from_id inverse_lerp is_equal_approx is_finite is_inf is_instance_id_valid is_instance_valid is_nan is_same is_zero_approx lerp lerp_angle lerpf linear_to_db log max maxf maxi min minf mini move_toward nearest_po2 pingpong posmod pow print print_rich print_verbose printerr printraw prints printt push_error push_warning rad_to_deg rand_from_seed randf randf_range randfn randi randi_range randomize remap rid_allocate_id rid_from_int64 round roundf roundi seed sign signf signi sin sinh smoothstep snapped snappedf snappedi sqrt step_decimals str str_to_var tan tanh typeof var_to_bytes var_to_bytes_with_objects var_to_str weakref wrap wrapf wrapi"

    gdscript_methods="Color8 assert char convert dict_to_inst get_stack inst_to_dict is_instance_of len load preload print_debug print_stack range type_exists"

    gdscript_constants="PI TAU INF NAN"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list gdscript_static_words $(join "${keywords} ${values} ${builtin_classes} ${builtin_methods} ${gdscript_methods} ${gdscript_constants}" ' ')"

    printf %s "
        add-highlighter shared/gdscript/code/ regex '\b($(join "${keywords}" '|'))\b'           0:keyword
        add-highlighter shared/gdscript/code/ regex '\b($(join "${values}" '|'))\b'             0:value
        add-highlighter shared/gdscript/code/ regex '\b($(join "${builtin_classes}" '|'))\b'    0:type
        add-highlighter shared/gdscript/code/ regex '\b($(join "${builtin_methods}" '|'))\b\('  1:builtin
        add-highlighter shared/gdscript/code/ regex '\b($(join "${gdscript_methods}" '|'))\b\(' 1:builtin
        add-highlighter shared/gdscript/code/ regex '\b($(join "${gdscript_constants}" '|'))\b' 0:keyword
    "
}
# nodes
add-highlighter shared/gdscript/code/ regex \$[\w/]+\b                           0:module
add-highlighter shared/gdscript/code/ regex \%\w+(?!/)\b                         0:string

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden gdscript-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*#\h* <ret> y jgh P }
    }
}

define-command -hidden gdscript-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k x s \h+$ <ret> d }
        # indent after line ending with :
        try %{ execute-keys -draft , k x <a-k> :$ <ret> <a-K> ^\h*# <ret> j <a-gt> }
        # deindent closing brace/bracket when after cursor (for arrays and dictionaries)
        try %< execute-keys -draft x <a-k> ^\h*[}\]] <ret> gh / [}\]] <ret> m <a-S> 1<a-&> >
    >
>

§
