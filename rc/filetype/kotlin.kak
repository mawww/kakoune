# References --------------------------------------------------------------------------------------- #
# Team: Yerlan & Kirk Duncan
#
# Kotlin 2020, Keywords and Operators, v1.4.0, viewed 9 September 2020, https://kotlinlang.org/docs/reference/keyword-reference.html
# Kdoc 2020, Documenting Kotlin Code, Block Tags, v1.4.0, viewed 9 September 2020, https://kotlinlang.org/docs/reference/kotlin-doc.html
# Oracle 2020, Java Platform, Standard Edition & Java Development Kit, Version 14 API Specification, viewed 8 September 2020, https://docs.oracle.com/en/java/javase/14/docs/api/index.html
#
# File types --------------------------------------------------------------------------------------- #
hook global BufCreate .*[.](kt|kts)  %{
  set-option buffer filetype kotlin
}

# Initialization ----------------------------------------------------------------------------------- #
hook global WinSetOption filetype=kotlin %{
  require-module kotlin

  set-option -add window static_words %opt{main_static_words}
  set-option -add window static_words %opt{docs_static_words}
  set-option -add window static_words %opt{error_static_words}
  set-option -add window static_words %opt{exception_static_words}

  # cleanup trailing whitespaces when exiting insert mode
  hook window ModeChange pop:insert:.* -group kotlin-trim-indent %{ try %{ execute-keys -draft <a-x>s^\h+$<ret>d } }
  hook window InsertChar \n -group kotlin-indent kotlin-indent-on-new-line
  hook window InsertChar \{ -group kotlin-indent kotlin-indent-on-opening-curly-brace
  hook window InsertChar \} -group kotlin-indent kotlin-indent-on-closing-curly-brace

  hook -once -always window WinSetOption filetype=.* %{ remove-hooks window kotlin-.+ }
}

hook -group kotlin-highlighter global WinSetOption filetype=kotlin %{
  add-highlighter window/kotlin ref kotlin
  add-highlighter window/ktor ref ktor
  add-highlighter window/kdoc ref kdoc

  hook -once -always window WinSetOption filetype=.* %{
    remove-highlighter window/kotlin
    remove-highlighter window/ktor
    remove-highlighter window/kdoc
  }
}

hook global BufSetOption filetype=kotlin %{
  require-module kotlin

  set-option buffer comment_line '//'
  set-option buffer comment_block_begin '/*'
  set-option buffer comment_block_end '*/'

  hook -once -always buffer BufSetOption filetype=.* %{ remove-hooks buffer kotlin-.+ }
}

# Module ------------------------------------------------------------------------------------------- #
provide-module kotlin

add-highlighter shared/kotlin regions
add-highlighter shared/kotlin/code default-region group
add-highlighter shared/kotlin/string region %{(?<!')"} %{(?<!\\)(\\\\)*"} fill string
add-highlighter shared/kotlin/comment region /\* \*/ fill comment
add-highlighter shared/kotlin/inline_documentation region /// $ fill documentation
add-highlighter shared/kotlin/line_comment region // $ fill comment

add-highlighter shared/kotlin/code/kotlin_annotations regex @\w+\b|\b\w+@(?=\{) 0:meta
add-highlighter shared/kotlin/code/kotlin_identifiers regex \b(field|it)\b 1:variable
add-highlighter shared/kotlin/code/kotlin_fields      regex \.([A-Za-z_][\w]*)\s*?\. 1:type

# As at 15 March 2021, kotlin_method see: https://regex101.com/r/Mhy4HG/1
add-highlighter shared/kotlin/code/kotlin_methods     regex ::([A-Za-z_][\w]*)|\.([A-Za-z_][\w]*)\s*?[\(\{]|\.([A-Za-z_][\w]*)[\s\)\}>](?=[^\(\{]) 1:function 2:function 3:function

# Test suite functions: fun `this is a valid character function test`()
add-highlighter shared/kotlin/code/kotlin_fun_tests   regex ^\h*?fun\s*?`(.[^<>\[\]\\\.]+?)`\h*?(?=\() 1:default+uF
add-highlighter shared/kotlin/code/kotlin_delimiters  regex (\(|\)|\[|\]|\{|\}|\;|') 1:operator
add-highlighter shared/kotlin/code/kotlin_operators   regex (\+|-|\*|&|=|\\|\?|%|\|-|!|\||->|\.|,|<|>|:|\^|/) 1:operator
add-highlighter shared/kotlin/code/kotlin_numbers     regex \b((0(x|X)[0-9a-fA-F]*)|(([0-9]+\.?[0-9]*)|(\.[0-9]+))((e|E)(\+|-)?[0-9]+)?)([LlFf])?\b 0:value

# Generics need improvement, as after a colon will match as a constant only.
# val program: IOU = XXXX; val cat: DOG = XXXX. matches IOU or DOG as a
# CONSTANT when it could be generics. See: https://regex101.com/r/VPO5LE/7
add-highlighter shared/kotlin/code/kotlin_constants_and_generics regex ((?<==\h)\([A-Z][A-Z0-9_]+\b(?=[<:\;])|\b(?<!<)[A-Z][A-Z0-9_]+(?=[^>\)]))\b|\b((?<!=\s)(?<!\.)[A-Z]+\d*?(?![\(\;:])(?=[,\)>\s]))\b 1:meta 2:type

add-highlighter shared/kotlin/code/kotlin_target regex @(delegate|field|file|get|param|property|receiver|set|setparam)(?=:) 0:meta
add-highlighter shared/kotlin/code/kotlin_soft   regex \b(by|catch|constructor|dynamic|finally|get|import|init|set|where)\b 1:keyword
add-highlighter shared/kotlin/code/kotlin_hard   regex \b(as|as\?|break|class|continue|do|else|false|for|fun|if|in|!in|interface|is|!is|null|object|package|return|super|this|throw|true|try|typealias|val|var|when|while)\b 1:keyword

add-highlighter shared/kotlin/code/kotlin_modifier regex \b(actual|abstract|annotation|companion|const|crossinline|data|enum|expect|external|final|infix|inline|inner|internal|lateinit|noinline|open|operator|out|override|private|protected|public|reified|sealed|suspend|tailrec|vararg)\b(?=[\s\n]) 1:attribute

add-highlighter shared/kotlin/code/kotlin_type regex \b(Annotation|Any|Boolean|BooleanArray|Byte|ByteArray|Char|Character|CharArray|CharSequence|Class|ClassLoader|Cloneable|Comparable|Compiler|DeprecationLevel|Double|DoubleArray|Enum|Float|FloatArray|Function|Int|IntArray|Integer|Lazy|LazyThreadSafetyMode|Long|LongArray|Math|Nothing|Number|Object|Package|Pair|Process|Runnable|Runtime|SecurityManager|Short|ShortArray|StackTraceElement|StrictMath|String|StringBuffer|System|Thread|ThreadGroup|ThreadLocal|Triple|Unit|Void)\b(?=[^<]) 1:type

# Kdoc --------------------------------------------------------------------------------------------- #
add-highlighter shared/kdoc group
add-highlighter shared/kdoc/tag regex \*(?:\s+)?(@(author|constructor|exception|param|property|receiver|return|sample|see|since|suppress|throws))\b 1:default+ui

# Discolour ---------------------------------------------------------------------------------------- #
add-highlighter shared/kotlin/code/discolour regex ^(package|import)(?S)(.+) 2:default+fa

# Commands ----------------------------------------------------------------------------------------- #
define-command -hidden kotlin-indent-on-new-line %~
  evaluate-commands -draft -itersel %<
    # preserve previous line indent
    try %{ execute-keys -draft <semicolon>K<a-&> }
    # indent after lines ending with { or (
    try %[ execute-keys -draft k<a-x> <a-k> [{(]\h*$ <ret> j<a-gt> ]
    # cleanup trailing white spaces on the previous line
    try %{ execute-keys -draft k<a-x> s \h+$ <ret>d }
    # align to opening paren of previous line
    try %{ execute-keys -draft [( <a-k> \A\([^\n]+\n[^\n]*\n?\z <ret> s \A\(\h*.|.\z <ret> '<a-;>' & }
    # copy // comments prefix
    try %{ execute-keys -draft <semicolon><c-s>k<a-x> s ^\h*\K/{2,} <ret> y<c-o>P<esc> }
    # indent after a pattern match on when/where statements
    try %[ execute-keys -draft k<a-x> <a-k> ^\h*(when|where).*$ <ret> j<a-gt> ]
    # indent after term on an expression
    try %[ execute-keys -draft k<a-x> <a-k> =\h*?$ <ret> j<a-gt> ]
    # indent after keywords
    try %[ execute-keys -draft <semicolon><a-F>)MB <a-k> \A(catch|do|else|for|if|try|while)\h*\(.*\)\h*\n\h*\n?\z <ret> s \A|.\z <ret> 1<a-&>1<a-space><a-gt> ]
    # deindent closing brace(s) when after cursor
    try %[ execute-keys -draft <a-x> <a-k> ^\h*[})] <ret> gh / [})] <ret> m <a-S> 1<a-&> ]
  >
~

define-command -hidden kotlin-indent-on-opening-curly-brace %[
  # align indent with opening paren when { is entered on a new line after the closing paren
  try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
]

define-command -hidden kotlin-indent-on-closing-curly-brace %[
  # align to opening curly brace when alone on a line
  try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
]

# Exceptions, Errors, and Types -------------------------------------------------------------------- #
# macro: 93le<a-;>i<ret><esc><esc>
evaluate-commands %sh{
  kotlin_types='Annotation Any Boolean BooleanArray Byte ByteArray Char Character CharArray
    CharSequence Class ClassLoader Cloneable Comparable Compiler DeprecationLevel Double
    DoubleArray Enum Float FloatArray Function Int IntArray Integer Lazy LazyThreadSafetyMode
    Long LongArray Math Nothing Number Object Package Pair Process Runnable Runtime
    SecurityManager Short ShortArray StackTraceElement StrictMath String StringBuffer System
    Thread ThreadGroup ThreadLocal Triple Unit Void'

  # ------------------------------------------------------------------------------------------------ #

  kdocs='author constructor exception param property receiver return sample see since suppress throws'

  # ------------------------------------------------------------------------------------------------ #

  kotlin_errors_ALL='Error AssertionError NotImplementedError OutOfMemoryError'

  kotlin_exceptions_ALL='CharacterCodingException IllegalCallableAccessException
    IllegalPropertyDelegateAccessException NoSuchPropertyException RuntimeException Throwable'

  # ------------------------------------------------------------------------------------------------ #

  java_errors_ALL='AbstractMethodError AnnotationFormatError AssertionError AWTError
    BootstrapMethodError ClassCircularityError ClassFormatError CoderMalfunctionError
    ExceptionInInitializerError FactoryConfigurationError GenericSignatureFormatError
    IllegalAccessError IncompatibleClassChangeError InstantiationError InternalError IOError
    JMXServerErrorExceptionâ€‹ LinkageError NoClassDefFoundError NoSuchFieldError NoSuchMethodError
    OutOfMemoryError RuntimeErrorException RuntimeErrorExceptionâ€‹ SchemaFactoryConfigurationError
    ServerErrorâ€‹ ServiceConfigurationError StackOverflowError ThreadDeath
    TransformerFactoryConfigurationError UnknownError UnsatisfiedLinkError
    UnsupportedClassVersionError VerifyError VirtualMachineError ZipError'

  # ------------------------------------------------------------------------------------------------ #

  java_exceptions_ALL='AbsentInformationException AcceptPendingException AccessControlException
    AccessDeniedException AccessException AccountException AccountExpiredException
    AccountLockedException AccountNotFoundException ActivateFailedException ActivationException
    AEADBadTagException AgentInitializationException AgentLoadException AlreadyBoundException
    AlreadyBoundException AlreadyConnectedException AnnotationTypeMismatchException
    ArithmeticException ArrayIndexOutOfBoundsException ArrayStoreException
    AsynchronousCloseException AtomicMoveNotSupportedException AttachNotSupportedException
    AttachOperationFailedException AttributeInUseException AttributeModificationException
    AttributeNotFoundException AuthenticationException AuthenticationException
    AuthenticationNotSupportedException AWTException BackingStoreException
    BadAttributeValueExpException BadBinaryOpValueExpException BadLocationException
    BadPaddingException BadStringOperationException BatchUpdateException BindException
    BrokenBarrierException BufferOverflowException BufferUnderflowException CancellationException
    CancelledKeyException CannotProceedException CannotRedoException CannotUndoException
    CardException CardNotPresentException CatalogException CertificateEncodingException
    CertificateEncodingException CertificateException CertificateException
    CertificateExpiredException CertificateExpiredException CertificateNotYetValidException
    CertificateNotYetValidException CertificateParsingException CertificateParsingException
    CertificateRevokedException CertPathBuilderException CertPathValidatorException
    CertStoreException ChangedCharSetException CharacterCodingException CharConversionException
    ClassCastException ClassInstallException ClassNotFoundException ClassNotLoadedException
    ClassNotPreparedException CloneNotSupportedException ClosedByInterruptException
    ClosedChannelException ClosedConnectionException ClosedDirectoryStreamException
    ClosedFileSystemException ClosedSelectorException ClosedWatchServiceException CMMException
    CommunicationException CompletionException ConcurrentModificationException
    ConfigurationException ConnectException ConnectException ConnectIOException
    ConnectionPendingException ContextNotEmptyException CredentialException
    CredentialExpiredException CredentialNotFoundException CRLException DataFormatException
    DataTruncation DatatypeConfigurationException DateTimeException DateTimeParseException
    DestroyFailedException DigestException DirectoryIteratorException DirectoryNotEmptyException
    DOMException DuplicateFormatFlagsException DuplicateRequestException EmptyStackException
    EngineTerminationException EnumConstantNotPresentException EOFException EvalException
    EventException Exception ExecutionControlException ExecutionException
    ExemptionMechanismException ExpandVetoException ExportException FailedLoginException
    FileAlreadyExistsException FileLockInterruptionException FileNotFoundException FilerException
    FileSystemAlreadyExistsException FileSystemException FileSystemLoopException
    FileSystemNotFoundException FindException FontFormatException
    FormatFlagsConversionMismatchException FormatterClosedException GeneralSecurityException
    GSSException HeadlessException HttpConnectTimeoutException HttpRetryException
    HttpTimeoutException IIOException IIOInvalidTreeException IllegalAccessException
    IllegalArgumentException IllegalBlockingModeException IllegalBlockSizeException
    IllegalCallerException IllegalChannelGroupException IllegalCharsetNameException
    IllegalClassFormatException IllegalComponentStateException IllegalConnectorArgumentsException
    IllegalFormatCodePointException IllegalFormatConversionException IllegalFormatException
    IllegalFormatFlagsException IllegalFormatPrecisionException IllegalFormatWidthException
    IllegalMonitorStateException IllegalPathStateException IllegalReceiveException
    IllegalSelectorException IllegalStateException IllegalThreadStateException
    IllegalUnbindException IllformedLocaleException ImagingOpException
    InaccessibleObjectException IncompatibleThreadStateException IncompleteAnnotationException
    InconsistentDebugInfoException IndexOutOfBoundsException InputMismatchException
    InstanceAlreadyExistsException InstanceNotFoundException InstantiationException
    InsufficientResourcesException InternalException InternalException
    InterruptedByTimeoutException InterruptedException InterruptedIOException
    InterruptedNamingException IntrospectionException IntrospectionException
    InvalidAlgorithmParameterException InvalidApplicationException
    InvalidAttributeIdentifierException InvalidAttributesException InvalidAttributeValueException
    InvalidAttributeValueException InvalidClassException InvalidCodeIndexException
    InvalidDnDOperationException InvalidKeyException InvalidKeyException InvalidKeySpecException
    InvalidLineNumberException InvalidMarkException InvalidMidiDataException
    InvalidModuleDescriptorException InvalidModuleException InvalidNameException
    InvalidObjectException InvalidOpenTypeException InvalidParameterException
    InvalidParameterSpecException InvalidPathException InvalidPreferencesFormatException
    InvalidPropertiesFormatException InvalidRelationIdException InvalidRelationServiceException
    InvalidRelationTypeException InvalidRequestStateException InvalidRoleInfoException
    InvalidRoleValueException InvalidSearchControlsException InvalidSearchFilterException
    InvalidStackFrameException InvalidStreamException InvalidTargetObjectTypeException
    InvalidTypeException InvocationException InvocationTargetException IOException JarException
    JarSignerException JMException JMRuntimeException JMXProviderException
    JMXServerErrorException JSException JShellException KeyAlreadyExistsException KeyException
    KeyManagementException KeySelectorException KeyStoreException LambdaConversionException
    LayerInstantiationException LdapReferralException LimitExceededException
    LineUnavailableException LinkException LinkLoopException ListenerNotFoundException
    LoginException LSException MalformedInputException MalformedLinkException
    MalformedObjectNameException MalformedParameterizedTypeException MalformedParametersException
    MalformedURLException MarshalException MarshalException MBeanException
    MBeanRegistrationException MidiUnavailableException MimeTypeParseException
    MirroredTypeException MirroredTypesException MissingFormatArgumentException
    MissingFormatWidthException MissingResourceException MonitorSettingException
    NameAlreadyBoundException NameNotFoundException NamingException NamingSecurityException
    NativeMethodException NegativeArraySizeException NoConnectionPendingException
    NoInitialContextException NoninvertibleTransformException NonReadableChannelException
    NonWritableChannelException NoPermissionException NoRouteToHostException
    NoSuchAlgorithmException NoSuchAttributeException NoSuchDynamicMethodException
    NoSuchElementException NoSuchFieldException NoSuchFileException NoSuchMechanismException
    NoSuchMethodException NoSuchObjectException NoSuchPaddingException NoSuchProviderException
    NotActiveException NotBoundException NotCompliantMBeanException NotContextException
    NotDirectoryException NotImplementedException NotLinkException NotSerializableException
    NotYetBoundException NotYetConnectedException NullPointerException NumberFormatException
    ObjectCollectedException ObjectStreamException OpenDataException
    OperationNotSupportedException OperationsException OptionalDataException
    OverlappingFileLockException ParseException ParserConfigurationException
    PartialResultException PatternSyntaxException PortUnreachableException PrinterAbortException
    PrinterException PrinterIOException PrintException PrivilegedActionException
    ProfileDataException PropertyVetoException ProtocolException ProviderException
    ProviderMismatchException ProviderNotFoundException RangeException RasterFormatException
    ReadOnlyBufferException ReadOnlyFileSystemException ReadPendingException ReferralException
    ReflectionException ReflectiveOperationException RefreshFailedException
    RejectedExecutionException RelationException RelationNotFoundException
    RelationServiceNotRegisteredException RelationTypeNotFoundException RemoteException
    ResolutionException ResolutionException RMISecurityException RoleInfoNotFoundException
    RoleNotFoundException RowSetWarning RunException RuntimeErrorException RuntimeException
    RuntimeMBeanException RuntimeOperationsException SaslException SAXException
    SAXNotRecognizedException SAXNotSupportedException SAXParseException SchemaViolationException
    ScriptException SecurityException SerialException ServerCloneException ServerError
    ServerException ServerNotActiveException ServerRuntimeException ServiceNotFoundException
    ServiceUnavailableException ShortBufferException ShutdownChannelGroupException
    SignatureException SizeLimitExceededException SkeletonMismatchException
    SkeletonNotFoundException SocketException SocketSecurityException SocketTimeoutException
    SPIResolutionException SQLClientInfoException SQLDataException SQLException
    SQLFeatureNotSupportedException SQLIntegrityConstraintViolationException
    SQLInvalidAuthorizationSpecException SQLNonTransientConnectionException
    SQLNonTransientException SQLRecoverableException SQLSyntaxErrorException SQLTimeoutException
    SQLTransactionRollbackException SQLTransientConnectionException SQLTransientException
    SQLWarning SSLException SSLHandshakeException SSLKeyException SSLPeerUnverifiedException
    SSLProtocolException StoppedException StreamCorruptedException StringConcatException
    StringIndexOutOfBoundsException StubNotFoundException SyncFactoryException
    SyncFailedException SyncProviderException TimeLimitExceededException TimeoutException
    TooManyListenersException TransformerConfigurationException TransformerException
    TransformException TransportTimeoutException TypeNotPresentException UncheckedIOException
    UndeclaredThrowableException UnexpectedException UnknownAnnotationValueException
    UnknownDirectiveException UnknownElementException UnknownEntityException
    UnknownFormatConversionException UnknownFormatFlagsException UnknownGroupException
    UnknownHostException UnknownHostException UnknownObjectException UnknownServiceException
    UnknownTypeException UnmappableCharacterException UnmarshalException
    UnmodifiableClassException UnmodifiableModuleException UnmodifiableSetException
    UnrecoverableEntryException UnrecoverableKeyException UnresolvedAddressException
    UnresolvedReferenceException UnsupportedAddressTypeException UnsupportedAudioFileException
    UnsupportedCallbackException UnsupportedCharsetException UnsupportedEncodingException
    UnsupportedFlavorException UnsupportedLookAndFeelException UnsupportedOperationException
    UnsupportedTemporalTypeException URIReferenceException URISyntaxException UserException
    UserPrincipalNotFoundException UTFDataFormatException VMCannotBeModifiedException
    VMDisconnectedException VMMismatchException VMOutOfMemoryException VMStartException
    WebSocketHandshakeException WriteAbortedException WritePendingException
    WrongMethodTypeException XAException XMLParseException XMLSignatureException
    XMLStreamException XPathException XPathException XPathExpressionException
    XPathFactoryConfigurationException XPathFunctionException ZipException ZoneRulesException'

  # ------------------------------------------------------------------------------------------------ #

  join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

  # ------------------------------------------------------------------------------------------------ #

  printf %s\\n "declare-option str-list main_static_words $(join "${kotlin_types}" ' ')"

  printf %s\\n "declare-option str-list docs_static_words $(join "${kdocs}" ' ')"

  printf %s\\n "declare-option str-list error_static_words $(join "${kotlin_errors_ALL} ${java_errors_ALL}" ' ')"

  printf %s\\n "declare-option str-list exception_static_words $(join "${kotlin_exceptions_ALL} ${java_exceptions_ALL}" ' ')"

  # ------------------------------------------------------------------------------------------------ #

  printf %s\\n "
    add-highlighter shared/kotlin/code/kotlin_errors_ALL regex \b($(join "${kotlin_errors_ALL}" '|'))\b 0:type
    add-highlighter shared/kotlin/code/kotlin_exceptions_ALL regex \b($(join "${kotlin_exceptions_ALL}" '|'))\b 0:type
    add-highlighter shared/kotlin/code/java_errors_ALL regex \b($(join "${java_errors_ALL}" '|'))\b 0:type
    add-highlighter shared/kotlin/code/java_exceptions_A regex \b($(join "${java_exceptions_ALL}" '|'))\b 0:type
  "
}
