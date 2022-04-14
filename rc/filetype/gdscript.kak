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
add-highlighter shared/gdscript/double_string region '"'   (?<!\\)(\\\\)*"  fill string
add-highlighter shared/gdscript/single_string region "'"   (?<!\\)(\\\\)*'  fill string
add-highlighter shared/gdscript/comment       region '#'   '$'              fill comment

# integers
add-highlighter shared/gdscript/code/ regex '(?i)\b0b[01]+l?\b'               0:value
add-highlighter shared/gdscript/code/ regex '(?i)\b0x[\da-f]+l?\b'            0:value
add-highlighter shared/gdscript/code/ regex '(?i)\b0o?[0-7]+l?\b'             0:value
add-highlighter shared/gdscript/code/ regex '(?i)\b([1-9]\d*|0)l?\b'          0:value
# floats
add-highlighter shared/gdscript/code/ regex '\b\d+[eE][+-]?\d+\b'             0:value
add-highlighter shared/gdscript/code/ regex '(\b\d+)?\.\d+\b'                 0:value
add-highlighter shared/gdscript/code/ regex '\b\d+\.'                         0:value
# functions
add-highlighter shared/gdscript/code/ regex _?[a-zA-Z]\w*\s*(?=\()            0:function
add-highlighter shared/gdscript/code/ regex (?:func\h+)(_?\w+)(?:<[^>]+?>)?\( 1:function

add-highlighter shared/gdscript/code/ regex '(?:\+|-|\*|/|%|=|<|>|&|\||\^|~|:=)' 0:operator

evaluate-commands %sh{
    keywords="if elif else for while match break continue pass return class
    class_name extends is as self tool signal func static const enum var onready
    export setget breakpoint preload yield assert remote master puppet
    remotesync mastersync puppetsync PI TAU INF NAN"

    values="false true null"

    types="bool int float void null Vector2 Rect2 Vector3 Transform2D Plane Quat
    AABB Basis Transform Color NodePath RID Object Array Dictionary"

    builtin_classes="@GDScript @GlobalScope AABB AcceptDialog AnimatedSprite AnimatedSprite3D AnimatedTexture Animation AnimationNode AnimationNodeAdd2 AnimationNodeAdd3 AnimationNodeAnimation AnimationNodeBlend2 AnimationNodeBlend3 AnimationNodeBlendSpace1D AnimationNodeBlendSpace2D AnimationNodeBlendTree AnimationNodeOneShot AnimationNodeOutput AnimationNodeStateMachine AnimationNodeStateMachinePlayback AnimationNodeStateMachineTransition AnimationNodeTimeScale AnimationNodeTimeSeek AnimationNodeTransition AnimationPlayer AnimationRootNode AnimationTrackEditPlugin AnimationTree AnimationTreePlayer Area Area2D Array ArrayMesh ARVRAnchor ARVRCamera ARVRController ARVRInterface ARVRInterfaceGDNative ARVROrigin ARVRPositionalTracker ARVRServer AspectRatioContainer AStar AStar2D AtlasTexture AudioBusLayout AudioEffect AudioEffectAmplify AudioEffectBandLimitFilter AudioEffectBandPassFilter AudioEffectCapture AudioEffectChorus AudioEffectCompressor AudioEffectDelay AudioEffectDistortion AudioEffectEQ AudioEffectEQ10 AudioEffectEQ21 AudioEffectEQ6 AudioEffectFilter AudioEffectHighPassFilter AudioEffectHighShelfFilter AudioEffectInstance AudioEffectLimiter AudioEffectLowPassFilter AudioEffectLowShelfFilter AudioEffectNotchFilter AudioEffectPanner AudioEffectPhaser AudioEffectPitchShift AudioEffectRecord AudioEffectReverb AudioEffectSpectrumAnalyzer AudioEffectSpectrumAnalyzerInstance AudioEffectStereoEnhance AudioServer AudioStream AudioStreamGenerator AudioStreamGeneratorPlayback AudioStreamMicrophone AudioStreamMP3 AudioStreamOGGVorbis AudioStreamPlayback AudioStreamPlaybackResampled AudioStreamPlayer AudioStreamPlayer2D AudioStreamPlayer3D AudioStreamRandomPitch AudioStreamSample BackBufferCopy BakedLightmap BakedLightmapData BaseButton Basis BitMap BitmapFont Bone2D BoneAttachment bool BoxContainer BoxShape BulletPhysicsServer Button ButtonGroup Camera Camera2D CameraFeed CameraServer CameraTexture CanvasItem CanvasItemMaterial CanvasLayer CanvasModulate CapsuleMesh CapsuleShape CapsuleShape2D CenterContainer CharFXTransform CheckBox CheckButton CircleShape2D ClassDB ClippedCamera CollisionObject CollisionObject2D CollisionPolygon CollisionPolygon2D CollisionShape CollisionShape2D Color ColorPicker ColorPickerButton ColorRect ConcavePolygonShape ConcavePolygonShape2D ConeTwistJoint ConfigFile ConfirmationDialog Container Control ConvexPolygonShape ConvexPolygonShape2D CPUParticles CPUParticles2D Crypto CryptoKey CSGBox CSGCombiner CSGCylinder CSGMesh CSGPolygon CSGPrimitive CSGShape CSGSphere CSGTorus CSharpScript CubeMap CubeMesh Curve Curve2D Curve3D CurveTexture CylinderMesh CylinderShape DampedSpringJoint2D Dictionary DirectionalLight Directory DTLSServer DynamicFont DynamicFontData EditorExportPlugin EditorFeatureProfile EditorFileDialog EditorFileSystem EditorFileSystemDirectory EditorImportPlugin EditorInspector EditorInspectorPlugin EditorInterface EditorNavigationMeshGenerator EditorPlugin EditorProperty EditorResourceConversionPlugin EditorResourcePreview EditorResourcePreviewGenerator EditorSceneImporter EditorSceneImporterFBX EditorScenePostImport EditorScript EditorSelection EditorSettings EditorSpatialGizmo EditorSpatialGizmoPlugin EditorSpinSlider EditorVCSInterface EncodedObjectAsID Engine Environment Expression ExternalTexture File FileDialog FileSystemDock float Font FuncRef GDNative GDNativeLibrary GDScript GDScriptFunctionState Generic6DOFJoint Geometry GeometryInstance GIProbe GIProbeData GodotSharp Gradient GradientTexture GraphEdit GraphNode GridContainer GridMap GrooveJoint2D HashingContext HBoxContainer HeightMapShape HingeJoint HScrollBar HSeparator HSlider HSplitContainer HTTPClient HTTPRequest Image ImageTexture ImmediateGeometry Input InputEvent InputEventAction InputEventGesture InputEventJoypadButton InputEventJoypadMotion InputEventKey InputEventMagnifyGesture InputEventMIDI InputEventMouse InputEventMouseButton InputEventMouseMotion InputEventPanGesture InputEventScreenDrag InputEventScreenTouch InputEventWithModifiers InputMap InstancePlaceholder int InterpolatedCamera IP ItemList JavaClass JavaClassWrapper JavaScript JNISingleton Joint Joint2D JSON JSONParseResult JSONRPC KinematicBody KinematicBody2D KinematicCollision KinematicCollision2D Label LargeTexture Light Light2D LightOccluder2D Line2D LineEdit LineShape2D LinkButton Listener MainLoop MarginContainer Marshalls Material MenuButton Mesh MeshDataTool MeshInstance MeshInstance2D MeshLibrary MeshTexture MobileVRInterface MultiMesh MultiMeshInstance MultiMeshInstance2D MultiplayerAPI MultiplayerPeerGDNative Mutex NativeScript Navigation Navigation2D NavigationMesh NavigationMeshInstance NavigationPolygon NavigationPolygonInstance NetworkedMultiplayerENet NetworkedMultiplayerPeer NinePatchRect Node Node2D NodePath NoiseTexture Object OccluderPolygon2D OmniLight OpenSimplexNoise OptionButton OS PackedDataContainer PackedDataContainerRef PackedScene PacketPeer PacketPeerDTLS PacketPeerGDNative PacketPeerStream PacketPeerUDP Panel PanelContainer PanoramaSky ParallaxBackground ParallaxLayer Particles Particles2D ParticlesMaterial Path Path2D PathFollow PathFollow2D PCKPacker Performance PHashTranslation PhysicalBone Physics2DDirectBodyState Physics2DDirectSpaceState Physics2DServer Physics2DShapeQueryParameters Physics2DShapeQueryResult Physics2DTestMotionResult PhysicsBody PhysicsBody2D PhysicsDirectBodyState PhysicsDirectSpaceState PhysicsMaterial PhysicsServer PhysicsShapeQueryParameters PhysicsShapeQueryResult PinJoint PinJoint2D Plane PlaneMesh PlaneShape PluginScript PointMesh Polygon2D PolygonPathFinder PoolByteArray PoolColorArray PoolIntArray PoolRealArray PoolStringArray PoolVector2Array PoolVector3Array Popup PopupDialog PopupMenu PopupPanel Position2D Position3D PrimitiveMesh PrismMesh ProceduralSky ProgressBar ProjectSettings ProximityGroup ProxyTexture QuadMesh Quat RandomNumberGenerator Range RayCast RayCast2D RayShape RayShape2D Rect2 RectangleShape2D Reference ReferenceRect ReflectionProbe RegEx RegExMatch RemoteTransform RemoteTransform2D Resource ResourceFormatLoader ResourceFormatSaver ResourceImporter ResourceInteractiveLoader ResourceLoader ResourcePreloader ResourceSaver RichTextEffect RichTextLabel RID RigidBody RigidBody2D RootMotionView SceneState SceneTree SceneTreeTimer Script ScriptCreateDialog ScriptEditor ScrollBar ScrollContainer SegmentShape2D Semaphore Separator Shader ShaderMaterial Shape Shape2D ShortCut Skeleton Skeleton2D SkeletonIK Skin SkinReference Sky Slider SliderJoint SoftBody Spatial SpatialGizmo SpatialMaterial SpatialVelocityTracker SphereMesh SphereShape SpinBox SplitContainer SpotLight SpringArm Sprite Sprite3D SpriteBase3D SpriteFrames StaticBody StaticBody2D StreamPeer StreamPeerBuffer StreamPeerGDNative StreamPeerSSL StreamPeerTCP StreamTexture String StyleBox StyleBoxEmpty StyleBoxFlat StyleBoxLine StyleBoxTexture SurfaceTool TabContainer Tabs TCP_Server TextEdit TextFile Texture Texture3D TextureArray TextureButton TextureLayered TextureProgress TextureRect Theme Thread TileMap TileSet Timer ToolButton TouchScreenButton Transform Transform2D Translation TranslationServer Tree TreeItem TriangleMesh Tween UDPServer UndoRedo UPNP UPNPDevice Variant VBoxContainer Vector2 Vector3 VehicleBody VehicleWheel VideoPlayer VideoStream VideoStreamGDNative VideoStreamTheora VideoStreamWebm Viewport ViewportContainer ViewportTexture VisibilityEnabler VisibilityEnabler2D VisibilityNotifier VisibilityNotifier2D VisualInstance VisualScript VisualScriptBasicTypeConstant VisualScriptBuiltinFunc VisualScriptClassConstant VisualScriptComment VisualScriptComposeArray VisualScriptCondition VisualScriptConstant VisualScriptConstructor VisualScriptCustomNode VisualScriptDeconstruct VisualScriptEditor VisualScriptEmitSignal VisualScriptEngineSingleton VisualScriptExpression VisualScriptFunction VisualScriptFunctionCall VisualScriptFunctionState VisualScriptGlobalConstant VisualScriptIndexGet VisualScriptIndexSet VisualScriptInputAction VisualScriptIterator VisualScriptLists VisualScriptLocalVar VisualScriptLocalVarSet VisualScriptMathConstant VisualScriptNode VisualScriptOperator VisualScriptPreload VisualScriptPropertyGet VisualScriptPropertySet VisualScriptResourcePath VisualScriptReturn VisualScriptSceneNode VisualScriptSceneTree VisualScriptSelect VisualScriptSelf VisualScriptSequence VisualScriptSubCall VisualScriptSwitch VisualScriptTypeCast VisualScriptVariableGet VisualScriptVariableSet VisualScriptWhile VisualScriptYield VisualScriptYieldSignal VisualServer VisualShader VisualShaderNode VisualShaderNodeBooleanConstant VisualShaderNodeBooleanUniform VisualShaderNodeColorConstant VisualShaderNodeColorFunc VisualShaderNodeColorOp VisualShaderNodeColorUniform VisualShaderNodeCompare VisualShaderNodeCubeMap VisualShaderNodeCubeMapUniform VisualShaderNodeCustom VisualShaderNodeDeterminant VisualShaderNodeDotProduct VisualShaderNodeExpression VisualShaderNodeFaceForward VisualShaderNodeFresnel VisualShaderNodeGlobalExpression VisualShaderNodeGroupBase VisualShaderNodeIf VisualShaderNodeInput VisualShaderNodeIs VisualShaderNodeOuterProduct VisualShaderNodeOutput VisualShaderNodeScalarClamp VisualShaderNodeScalarConstant VisualShaderNodeScalarDerivativeFunc VisualShaderNodeScalarFunc VisualShaderNodeScalarInterp VisualShaderNodeScalarOp VisualShaderNodeScalarSmoothStep VisualShaderNodeScalarSwitch VisualShaderNodeScalarUniform VisualShaderNodeSwitch VisualShaderNodeTexture VisualShaderNodeTextureUniform VisualShaderNodeTextureUniformTriplanar VisualShaderNodeTransformCompose VisualShaderNodeTransformConstant VisualShaderNodeTransformDecompose VisualShaderNodeTransformFunc VisualShaderNodeTransformMult VisualShaderNodeTransformUniform VisualShaderNodeTransformVecMult VisualShaderNodeUniform VisualShaderNodeUniformRef VisualShaderNodeVec3Constant VisualShaderNodeVec3Uniform VisualShaderNodeVectorClamp VisualShaderNodeVectorCompose VisualShaderNodeVectorDecompose VisualShaderNodeVectorDerivativeFunc VisualShaderNodeVectorDistance VisualShaderNodeVectorFunc VisualShaderNodeVectorInterp VisualShaderNodeVectorLen VisualShaderNodeVectorOp VisualShaderNodeVectorRefract VisualShaderNodeVectorScalarMix VisualShaderNodeVectorScalarSmoothStep VisualShaderNodeVectorScalarStep VisualShaderNodeVectorSmoothStep VScrollBar VSeparator VSlider VSplitContainer WeakRef WebRTCDataChannel WebRTCDataChannelGDNative WebRTCMultiplayer WebRTCPeerConnection WebRTCPeerConnectionGDNative WebSocketClient WebSocketMultiplayerPeer WebSocketPeer WebSocketServer WebXRInterface WindowDialog World World2D WorldEnvironment X509Certificate XMLParser YSort"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list gdscript_static_words $(join "${keywords} ${values} ${types} ${builtin_classes}" ' ')"

    printf %s "
        add-highlighter shared/gdscript/code/ regex '\b($(join "${keywords}" '|'))\b' 0:keyword
        add-highlighter shared/gdscript/code/ regex '\b($(join "${values}" '|'))\b' 0:value
        add-highlighter shared/gdscript/code/ regex '\b($(join "${types}" '|'))\b' 0:type
        add-highlighter shared/gdscript/code/ regex '\b($(join "${builtin_classes}" '|'))\b' 0:type
    "
}

# nodes
add-highlighter shared/gdscript/code/ regex '\$[\w/]*'                        0:module


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
