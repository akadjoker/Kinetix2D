 

//#define _CRT_SECURE_NO_WARNINGS
#include "OpenGLES3.h"
#include <cstdlib>
#include <cstring>

// GLES3_LOADER_ONLY: the application always gives a loader (glesSetProcAddressLoader),
// so neither EGL nor dlsym is needed (a WGL or GLX context has no EGL).
#if defined( GLES3_LOADER_ONLY )
#elif defined( __APPLE__ )
#include <dlfcn.h>
#elif defined( __EMSCRIPTEN__ )
#include <EGL/egl.h>
#else
#include <EGL/egl.h>
#if !defined( _WIN32 ) && !defined( _WIN32_WCE )
#include <dlfcn.h>
#endif
#endif

using namespace gles;

namespace glESExt
{
	bool EXT_texture_filter_anisotropic = false;
	bool EXT_disjoint_timer_query = false;
	
	bool  EXT_color_buffer_float = false;

	bool EXT_geometry_shader = false;
	bool EXT_tessellation_shader = false;

	bool EXT_texture_compression_s3tc = false;
	bool EXT_texture_compression_dxt1 = false;
	bool EXT_texture_compression_bptc = false;
	bool KHR_texture_compression_astc = false;

	bool OES_compressed_ETC1_RGB8_texture = false;

	bool EXT_texture_border_clamp = false;
	bool OES_texture_3D = false;

	bool OES_EGL_image_external = false;
	bool KHR_debug = false;
	
	int	majorVersion = 1, minorVersion = 0;
}



namespace gles
{
	PFNGLQUERYCOUNTEREXTPROC glQueryCounterEXT = 0x0;
	PFNGLGETQUERYOBJECTIVEXTPROC glGetQueryObjectivEXT = 0x0;
	PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64vEXT = 0x0;

	PFNGLTEXPARAMETERIIVEXTPROC glTexParameterIivEXT = 0x0;
	PFNGLTEXPARAMETERIUIVEXTPROC glTexParameterIuivEXT = 0x0;
	PFNGLGETTEXPARAMETERIIVEXTPROC glGetTexParameterIivEXT = 0x0;
	PFNGLGETTEXPARAMETERIUIVEXTPROC glGetTexParameterIuivEXT = 0x0;
	PFNGLSAMPLERPARAMETERIIVEXTPROC glSamplerParameterIivEXT = 0x0;
	PFNGLSAMPLERPARAMETERIUIVEXTPROC glSamplerParameterIuivEXT = 0x0;
	PFNGLGETSAMPLERPARAMETERIIVEXTPROC glGetSamplerParameterIivEXT = 0x0;
	PFNGLGETSAMPLERPARAMETERIUIVEXTPROC glGetSamplerParameterIuivEXT = 0x0;

	PFNGLFRAMEBUFFERTEXTURE3DOESPROC glFramebufferTexture3DOES = 0x0;
	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = 0x0;

	PFNGLDEBUGMESSAGECONTROLKHRPROC glDebugMessageControlKHR = 0x0;
	PFNGLDEBUGMESSAGEINSERTKHRPROC glDebugMessageInsertKHR = 0x0;
	PFNGLDEBUGMESSAGECALLBACKKHRPROC glDebugMessageCallbackKHR = 0x0;
	PFNGLGETDEBUGMESSAGELOGKHRPROC glGetDebugMessageLogKHR = 0x0;

	// Core ES 2.0
	PFNGLACTIVETEXTUREPROC glActiveTexture = 0x0;
	PFNGLATTACHSHADERPROC glAttachShader = 0x0;
	PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation = 0x0;
	PFNGLBINDBUFFERPROC glBindBuffer = 0x0;
	PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = 0x0;
	PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer = 0x0;
	PFNGLBINDTEXTUREPROC glBindTexture = 0x0;
	PFNGLBLENDCOLORPROC glBlendColor = 0x0;
	PFNGLBLENDEQUATIONPROC glBlendEquation = 0x0;
	PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate = 0x0;
	PFNGLBLENDFUNCPROC glBlendFunc = 0x0;
	PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate = 0x0;
	PFNGLBUFFERDATAPROC glBufferData = 0x0;
	PFNGLBUFFERSUBDATAPROC glBufferSubData = 0x0;
	PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = 0x0;
	PFNGLCLEARPROC glClear = 0x0;
	PFNGLCLEARCOLORPROC glClearColor = 0x0;
	PFNGLCLEARDEPTHFPROC glClearDepthf = 0x0;
	PFNGLCLEARSTENCILPROC glClearStencil = 0x0;
	PFNGLCOLORMASKPROC glColorMask = 0x0;
	PFNGLCOMPILESHADERPROC glCompileShader = 0x0;
	PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D = 0x0;
	PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D = 0x0;
	PFNGLCOPYTEXIMAGE2DPROC glCopyTexImage2D = 0x0;
	PFNGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D = 0x0;
	PFNGLCREATEPROGRAMPROC glCreateProgram = 0x0;
	PFNGLCREATESHADERPROC glCreateShader = 0x0;
	PFNGLCULLFACEPROC glCullFace = 0x0;
	PFNGLDELETEBUFFERSPROC glDeleteBuffers = 0x0;
	PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = 0x0;
	PFNGLDELETEPROGRAMPROC glDeleteProgram = 0x0;
	PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers = 0x0;
	PFNGLDELETESHADERPROC glDeleteShader = 0x0;
	PFNGLDELETETEXTURESPROC glDeleteTextures = 0x0;
	PFNGLDEPTHFUNCPROC glDepthFunc = 0x0;
	PFNGLDEPTHMASKPROC glDepthMask = 0x0;
	PFNGLDEPTHRANGEFPROC glDepthRangef = 0x0;
	PFNGLDETACHSHADERPROC glDetachShader = 0x0;
	PFNGLDISABLEPROC glDisable = 0x0;
	PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = 0x0;
	PFNGLDRAWARRAYSPROC glDrawArrays = 0x0;
	PFNGLDRAWELEMENTSPROC glDrawElements = 0x0;
	PFNGLENABLEPROC glEnable = 0x0;
	PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = 0x0;
	PFNGLFINISHPROC glFinish = 0x0;
	PFNGLFLUSHPROC glFlush = 0x0;
	PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer = 0x0;
	PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = 0x0;
	PFNGLFRONTFACEPROC glFrontFace = 0x0;
	PFNGLGENBUFFERSPROC glGenBuffers = 0x0;
	PFNGLGENERATEMIPMAPPROC glGenerateMipmap = 0x0;
	PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = 0x0;
	PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers = 0x0;
	PFNGLGENTEXTURESPROC glGenTextures = 0x0;
	PFNGLGETACTIVEATTRIBPROC glGetActiveAttrib = 0x0;
	PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform = 0x0;
	PFNGLGETATTACHEDSHADERSPROC glGetAttachedShaders = 0x0;
	PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = 0x0;
	PFNGLGETBOOLEANVPROC glGetBooleanv = 0x0;
	PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv = 0x0;
	PFNGLGETERRORPROC glGetError = 0x0;
	PFNGLGETFLOATVPROC glGetFloatv = 0x0;
	PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameteriv = 0x0;
	PFNGLGETINTEGERVPROC glGetIntegerv = 0x0;
	PFNGLGETPROGRAMIVPROC glGetProgramiv = 0x0;
	PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = 0x0;
	PFNGLGETRENDERBUFFERPARAMETERIVPROC glGetRenderbufferParameteriv = 0x0;
	PFNGLGETSHADERIVPROC glGetShaderiv = 0x0;
	PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = 0x0;
	PFNGLGETSHADERPRECISIONFORMATPROC glGetShaderPrecisionFormat = 0x0;
	PFNGLGETSHADERSOURCEPROC glGetShaderSource = 0x0;
	PFNGLGETSTRINGPROC glGetString = 0x0;
	PFNGLGETTEXPARAMETERFVPROC glGetTexParameterfv = 0x0;
	PFNGLGETTEXPARAMETERIVPROC glGetTexParameteriv = 0x0;
	PFNGLGETUNIFORMFVPROC glGetUniformfv = 0x0;
	PFNGLGETUNIFORMIVPROC glGetUniformiv = 0x0;
	PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = 0x0;
	PFNGLGETVERTEXATTRIBFVPROC glGetVertexAttribfv = 0x0;
	PFNGLGETVERTEXATTRIBIVPROC glGetVertexAttribiv = 0x0;
	PFNGLGETVERTEXATTRIBPOINTERVPROC glGetVertexAttribPointerv = 0x0;
	PFNGLHINTPROC glHint = 0x0;
	PFNGLISBUFFERPROC glIsBuffer = 0x0;
	PFNGLISENABLEDPROC glIsEnabled = 0x0;
	PFNGLISFRAMEBUFFERPROC glIsFramebuffer = 0x0;
	PFNGLISPROGRAMPROC glIsProgram = 0x0;
	PFNGLISRENDERBUFFERPROC glIsRenderbuffer = 0x0;
	PFNGLISSHADERPROC glIsShader = 0x0;
	PFNGLISTEXTUREPROC glIsTexture = 0x0;
	PFNGLLINEWIDTHPROC glLineWidth = 0x0;
	PFNGLLINKPROGRAMPROC glLinkProgram = 0x0;
	PFNGLPIXELSTOREIPROC glPixelStorei = 0x0;
	PFNGLPOLYGONOFFSETPROC glPolygonOffset = 0x0;
	PFNGLREADPIXELSPROC glReadPixels = 0x0;
	PFNGLRELEASESHADERCOMPILERPROC glReleaseShaderCompiler = 0x0;
	PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = 0x0;
	PFNGLSAMPLECOVERAGEPROC glSampleCoverage = 0x0;
	PFNGLSCISSORPROC glScissor = 0x0;
	PFNGLSHADERBINARYPROC glShaderBinary = 0x0;
	PFNGLSHADERSOURCEPROC glShaderSource = 0x0;
	PFNGLSTENCILFUNCPROC glStencilFunc = 0x0;
	PFNGLSTENCILFUNCSEPARATEPROC glStencilFuncSeparate = 0x0;
	PFNGLSTENCILMASKPROC glStencilMask = 0x0;
	PFNGLSTENCILMASKSEPARATEPROC glStencilMaskSeparate = 0x0;
	PFNGLSTENCILOPPROC glStencilOp = 0x0;
	PFNGLSTENCILOPSEPARATEPROC glStencilOpSeparate = 0x0;
	PFNGLTEXIMAGE2DPROC glTexImage2D = 0x0;
	PFNGLTEXPARAMETERFPROC glTexParameterf = 0x0;
	PFNGLTEXPARAMETERFVPROC glTexParameterfv = 0x0;
	PFNGLTEXPARAMETERIPROC glTexParameteri = 0x0;
	PFNGLTEXPARAMETERIVPROC glTexParameteriv = 0x0;
	PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D = 0x0;
	PFNGLUNIFORM1FPROC glUniform1f = 0x0;
	PFNGLUNIFORM1FVPROC glUniform1fv = 0x0;
	PFNGLUNIFORM1IPROC glUniform1i = 0x0;
	PFNGLUNIFORM1IVPROC glUniform1iv = 0x0;
	PFNGLUNIFORM2FPROC glUniform2f = 0x0;
	PFNGLUNIFORM2FVPROC glUniform2fv = 0x0;
	PFNGLUNIFORM2IPROC glUniform2i = 0x0;
	PFNGLUNIFORM2IVPROC glUniform2iv = 0x0;
	PFNGLUNIFORM3FPROC glUniform3f = 0x0;
	PFNGLUNIFORM3FVPROC glUniform3fv = 0x0;
	PFNGLUNIFORM3IPROC glUniform3i = 0x0;
	PFNGLUNIFORM3IVPROC glUniform3iv = 0x0;
	PFNGLUNIFORM4FPROC glUniform4f = 0x0;
	PFNGLUNIFORM4FVPROC glUniform4fv = 0x0;
	PFNGLUNIFORM4IPROC glUniform4i = 0x0;
	PFNGLUNIFORM4IVPROC glUniform4iv = 0x0;
	PFNGLUNIFORMMATRIX2FVPROC glUniformMatrix2fv = 0x0;
	PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv = 0x0;
	PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = 0x0;
	PFNGLUSEPROGRAMPROC glUseProgram = 0x0;
	PFNGLVALIDATEPROGRAMPROC glValidateProgram = 0x0;
	PFNGLVERTEXATTRIB1FPROC glVertexAttrib1f = 0x0;
	PFNGLVERTEXATTRIB1FVPROC glVertexAttrib1fv = 0x0;
	PFNGLVERTEXATTRIB2FPROC glVertexAttrib2f = 0x0;
	PFNGLVERTEXATTRIB2FVPROC glVertexAttrib2fv = 0x0;
	PFNGLVERTEXATTRIB3FPROC glVertexAttrib3f = 0x0;
	PFNGLVERTEXATTRIB3FVPROC glVertexAttrib3fv = 0x0;
	PFNGLVERTEXATTRIB4FPROC glVertexAttrib4f = 0x0;
	PFNGLVERTEXATTRIB4FVPROC glVertexAttrib4fv = 0x0;
	PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = 0x0;
	PFNGLVIEWPORTPROC glViewport = 0x0;

	// Core ES 3.0
	PFNGLREADBUFFERPROC glReadBuffer = 0x0;
	PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements = 0x0;
	PFNGLTEXIMAGE3DPROC glTexImage3D = 0x0;
	PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D = 0x0;
	PFNGLCOPYTEXSUBIMAGE3DPROC glCopyTexSubImage3D = 0x0;
	PFNGLCOMPRESSEDTEXIMAGE3DPROC glCompressedTexImage3D = 0x0;
	PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D = 0x0;
	PFNGLGENQUERIESPROC glGenQueries = 0x0;
	PFNGLDELETEQUERIESPROC glDeleteQueries = 0x0;
	PFNGLISQUERYPROC glIsQuery = 0x0;
	PFNGLBEGINQUERYPROC glBeginQuery = 0x0;
	PFNGLENDQUERYPROC glEndQuery = 0x0;
	PFNGLGETQUERYIVPROC glGetQueryiv = 0x0;
	PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv = 0x0;
	PFNGLUNMAPBUFFERPROC glUnmapBuffer = 0x0;
	PFNGLGETBUFFERPOINTERVPROC glGetBufferPointerv = 0x0;
	PFNGLDRAWBUFFERSPROC glDrawBuffers = 0x0;
	PFNGLUNIFORMMATRIX2X3FVPROC glUniformMatrix2x3fv = 0x0;
	PFNGLUNIFORMMATRIX3X2FVPROC glUniformMatrix3x2fv = 0x0;
	PFNGLUNIFORMMATRIX2X4FVPROC glUniformMatrix2x4fv = 0x0;
	PFNGLUNIFORMMATRIX4X2FVPROC glUniformMatrix4x2fv = 0x0;
	PFNGLUNIFORMMATRIX3X4FVPROC glUniformMatrix3x4fv = 0x0;
	PFNGLUNIFORMMATRIX4X3FVPROC glUniformMatrix4x3fv = 0x0;
	PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer = 0x0;
	PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample = 0x0;
	PFNGLFRAMEBUFFERTEXTURELAYERPROC glFramebufferTextureLayer = 0x0;
	PFNGLMAPBUFFERRANGEPROC glMapBufferRange = 0x0;
	PFNGLFLUSHMAPPEDBUFFERRANGEPROC glFlushMappedBufferRange = 0x0;
	PFNGLBINDVERTEXARRAYPROC glBindVertexArray = 0x0;
	PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = 0x0;
	PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = 0x0;
	PFNGLISVERTEXARRAYPROC glIsVertexArray = 0x0;
	PFNGLGETINTEGERI_VPROC glGetIntegeri_v = 0x0;
	PFNGLBEGINTRANSFORMFEEDBACKPROC glBeginTransformFeedback = 0x0;
	PFNGLENDTRANSFORMFEEDBACKPROC glEndTransformFeedback = 0x0;
	PFNGLBINDBUFFERRANGEPROC glBindBufferRange = 0x0;
	PFNGLBINDBUFFERBASEPROC glBindBufferBase = 0x0;
	PFNGLTRANSFORMFEEDBACKVARYINGSPROC glTransformFeedbackVaryings = 0x0;
	PFNGLGETTRANSFORMFEEDBACKVARYINGPROC glGetTransformFeedbackVarying = 0x0;
	PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer = 0x0;
	PFNGLGETVERTEXATTRIBIIVPROC glGetVertexAttribIiv = 0x0;
	PFNGLGETVERTEXATTRIBIUIVPROC glGetVertexAttribIuiv = 0x0;
	PFNGLVERTEXATTRIBI4IPROC glVertexAttribI4i = 0x0;
	PFNGLVERTEXATTRIBI4UIPROC glVertexAttribI4ui = 0x0;
	PFNGLVERTEXATTRIBI4IVPROC glVertexAttribI4iv = 0x0;
	PFNGLVERTEXATTRIBI4UIVPROC glVertexAttribI4uiv = 0x0;
	PFNGLGETUNIFORMUIVPROC glGetUniformuiv = 0x0;
	PFNGLGETFRAGDATALOCATIONPROC glGetFragDataLocation = 0x0;
	PFNGLUNIFORM1UIPROC glUniform1ui = 0x0;
	PFNGLUNIFORM2UIPROC glUniform2ui = 0x0;
	PFNGLUNIFORM3UIPROC glUniform3ui = 0x0;
	PFNGLUNIFORM4UIPROC glUniform4ui = 0x0;
	PFNGLUNIFORM1UIVPROC glUniform1uiv = 0x0;
	PFNGLUNIFORM2UIVPROC glUniform2uiv = 0x0;
	PFNGLUNIFORM3UIVPROC glUniform3uiv = 0x0;
	PFNGLUNIFORM4UIVPROC glUniform4uiv = 0x0;
	PFNGLCLEARBUFFERIVPROC glClearBufferiv = 0x0;
	PFNGLCLEARBUFFERUIVPROC glClearBufferuiv = 0x0;
	PFNGLCLEARBUFFERFVPROC glClearBufferfv = 0x0;
	PFNGLCLEARBUFFERFIPROC glClearBufferfi = 0x0;
	PFNGLGETSTRINGIPROC glGetStringi = 0x0;
	PFNGLCOPYBUFFERSUBDATAPROC glCopyBufferSubData = 0x0;
	PFNGLGETUNIFORMINDICESPROC glGetUniformIndices = 0x0;
	PFNGLGETACTIVEUNIFORMSIVPROC glGetActiveUniformsiv = 0x0;
	PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex = 0x0;
	PFNGLGETACTIVEUNIFORMBLOCKIVPROC glGetActiveUniformBlockiv = 0x0;
	PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glGetActiveUniformBlockName = 0x0;
	PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding = 0x0;
	PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced = 0x0;
	PFNGLDRAWELEMENTSINSTANCEDPROC glDrawElementsInstanced = 0x0;
	PFNGLFENCESYNCPROC glFenceSync = 0x0;
	PFNGLISSYNCPROC glIsSync = 0x0;
	PFNGLDELETESYNCPROC glDeleteSync = 0x0;
	PFNGLCLIENTWAITSYNCPROC glClientWaitSync = 0x0;
	PFNGLWAITSYNCPROC glWaitSync = 0x0;
	PFNGLGETINTEGER64VPROC glGetInteger64v = 0x0;
	PFNGLGETSYNCIVPROC glGetSynciv = 0x0;
	PFNGLGETINTEGER64I_VPROC glGetInteger64i_v = 0x0;
	PFNGLGETBUFFERPARAMETERI64VPROC glGetBufferParameteri64v = 0x0;
	PFNGLGENSAMPLERSPROC glGenSamplers = 0x0;
	PFNGLDELETESAMPLERSPROC glDeleteSamplers = 0x0;
	PFNGLISSAMPLERPROC glIsSampler = 0x0;
	PFNGLBINDSAMPLERPROC glBindSampler = 0x0;
	PFNGLSAMPLERPARAMETERIPROC glSamplerParameteri = 0x0;
	PFNGLSAMPLERPARAMETERIVPROC glSamplerParameteriv = 0x0;
	PFNGLSAMPLERPARAMETERFPROC glSamplerParameterf = 0x0;
	PFNGLSAMPLERPARAMETERFVPROC glSamplerParameterfv = 0x0;
	PFNGLGETSAMPLERPARAMETERIVPROC glGetSamplerParameteriv = 0x0;
	PFNGLGETSAMPLERPARAMETERFVPROC glGetSamplerParameterfv = 0x0;
	PFNGLVERTEXATTRIBDIVISORPROC glVertexAttribDivisor = 0x0;
	PFNGLBINDTRANSFORMFEEDBACKPROC glBindTransformFeedback = 0x0;
	PFNGLDELETETRANSFORMFEEDBACKSPROC glDeleteTransformFeedbacks = 0x0;
	PFNGLGENTRANSFORMFEEDBACKSPROC glGenTransformFeedbacks = 0x0;
	PFNGLISTRANSFORMFEEDBACKPROC glIsTransformFeedback = 0x0;
	PFNGLPAUSETRANSFORMFEEDBACKPROC glPauseTransformFeedback = 0x0;
	PFNGLRESUMETRANSFORMFEEDBACKPROC glResumeTransformFeedback = 0x0;
	PFNGLGETPROGRAMBINARYPROC glGetProgramBinary = 0x0;
	PFNGLPROGRAMBINARYPROC glProgramBinary = 0x0;
	PFNGLPROGRAMPARAMETERIPROC glProgramParameteri = 0x0;
	PFNGLINVALIDATEFRAMEBUFFERPROC glInvalidateFramebuffer = 0x0;
	PFNGLINVALIDATESUBFRAMEBUFFERPROC glInvalidateSubFramebuffer = 0x0;
	PFNGLTEXSTORAGE2DPROC glTexStorage2D = 0x0;
	PFNGLTEXSTORAGE3DPROC glTexStorage3D = 0x0;
	PFNGLGETINTERNALFORMATIVPROC glGetInternalformativ = 0x0;

	// Core ES 3.1
	PFNGLDISPATCHCOMPUTEPROC glDispatchCompute = 0x0;
	PFNGLGETPROGRAMRESOURCEINDEXPROC glGetProgramResourceIndex = 0x0;
	PFNGLGETPROGRAMRESOURCEIVPROC glGetProgramResourceiv = 0x0;
	PFNGLMEMORYBARRIERPROC glMemoryBarrier = 0x0;
	PFNGLDISPATCHCOMPUTEINDIRECTPROC glDispatchComputeIndirect = 0x0;
	PFNGLDRAWARRAYSINDIRECTPROC glDrawArraysIndirect = 0x0;
	PFNGLDRAWELEMENTSINDIRECTPROC glDrawElementsIndirect = 0x0;
	PFNGLFRAMEBUFFERPARAMETERIPROC glFramebufferParameteri = 0x0;
	PFNGLGETFRAMEBUFFERPARAMETERIVPROC glGetFramebufferParameteriv = 0x0;
	PFNGLGETPROGRAMINTERFACEIVPROC glGetProgramInterfaceiv = 0x0;
	PFNGLGETPROGRAMRESOURCENAMEPROC glGetProgramResourceName = 0x0;
	PFNGLGETPROGRAMRESOURCELOCATIONPROC glGetProgramResourceLocation = 0x0;
	PFNGLUSEPROGRAMSTAGESPROC glUseProgramStages = 0x0;
	PFNGLACTIVESHADERPROGRAMPROC glActiveShaderProgram = 0x0;
	PFNGLCREATESHADERPROGRAMVPROC glCreateShaderProgramv = 0x0;
	PFNGLBINDPROGRAMPIPELINEPROC glBindProgramPipeline = 0x0;
	PFNGLDELETEPROGRAMPIPELINESPROC glDeleteProgramPipelines = 0x0;
	PFNGLGENPROGRAMPIPELINESPROC glGenProgramPipelines = 0x0;
	PFNGLISPROGRAMPIPELINEPROC glIsProgramPipeline = 0x0;
	PFNGLGETPROGRAMPIPELINEIVPROC glGetProgramPipelineiv = 0x0;
	PFNGLPROGRAMUNIFORM1IPROC glProgramUniform1i = 0x0;
	PFNGLPROGRAMUNIFORM2IPROC glProgramUniform2i = 0x0;
	PFNGLPROGRAMUNIFORM3IPROC glProgramUniform3i = 0x0;
	PFNGLPROGRAMUNIFORM4IPROC glProgramUniform4i = 0x0;
	PFNGLPROGRAMUNIFORM1UIPROC glProgramUniform1ui = 0x0;
	PFNGLPROGRAMUNIFORM2UIPROC glProgramUniform2ui = 0x0;
	PFNGLPROGRAMUNIFORM3UIPROC glProgramUniform3ui = 0x0;
	PFNGLPROGRAMUNIFORM4UIPROC glProgramUniform4ui = 0x0;
	PFNGLPROGRAMUNIFORM1FPROC glProgramUniform1f = 0x0;
	PFNGLPROGRAMUNIFORM2FPROC glProgramUniform2f = 0x0;
	PFNGLPROGRAMUNIFORM3FPROC glProgramUniform3f = 0x0;
	PFNGLPROGRAMUNIFORM4FPROC glProgramUniform4f = 0x0;
	PFNGLPROGRAMUNIFORM1IVPROC glProgramUniform1iv = 0x0;
	PFNGLPROGRAMUNIFORM2IVPROC glProgramUniform2iv = 0x0;
	PFNGLPROGRAMUNIFORM3IVPROC glProgramUniform3iv = 0x0;
	PFNGLPROGRAMUNIFORM4IVPROC glProgramUniform4iv = 0x0;
	PFNGLPROGRAMUNIFORM1UIVPROC glProgramUniform1uiv = 0x0;
	PFNGLPROGRAMUNIFORM2UIVPROC glProgramUniform2uiv = 0x0;
	PFNGLPROGRAMUNIFORM3UIVPROC glProgramUniform3uiv = 0x0;
	PFNGLPROGRAMUNIFORM4UIVPROC glProgramUniform4uiv = 0x0;
	PFNGLPROGRAMUNIFORM1FVPROC glProgramUniform1fv = 0x0;
	PFNGLPROGRAMUNIFORM2FVPROC glProgramUniform2fv = 0x0;
	PFNGLPROGRAMUNIFORM3FVPROC glProgramUniform3fv = 0x0;
	PFNGLPROGRAMUNIFORM4FVPROC glProgramUniform4fv = 0x0;
	PFNGLPROGRAMUNIFORMMATRIX2FVPROC glProgramUniformMatrix2fv = 0x0;
	PFNGLPROGRAMUNIFORMMATRIX3FVPROC glProgramUniformMatrix3fv = 0x0;
	PFNGLPROGRAMUNIFORMMATRIX4FVPROC glProgramUniformMatrix4fv = 0x0;
	PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC glProgramUniformMatrix2x3fv = 0x0;
	PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC glProgramUniformMatrix3x2fv = 0x0;
	PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC glProgramUniformMatrix2x4fv = 0x0;
	PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC glProgramUniformMatrix4x2fv = 0x0;
	PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC glProgramUniformMatrix3x4fv = 0x0;
	PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC glProgramUniformMatrix4x3fv = 0x0;
	PFNGLVALIDATEPROGRAMPIPELINEPROC glValidateProgramPipeline = 0x0;
	PFNGLGETPROGRAMPIPELINEINFOLOGPROC glGetProgramPipelineInfoLog = 0x0;
	PFNGLBINDIMAGETEXTUREPROC glBindImageTexture = 0x0;
	PFNGLGETBOOLEANI_VPROC glGetBooleani_v = 0x0;
	PFNGLMEMORYBARRIERBYREGIONPROC glMemoryBarrierByRegion = 0x0;
	PFNGLTEXSTORAGE2DMULTISAMPLEPROC glTexStorage2DMultisample = 0x0;
	PFNGLGETMULTISAMPLEFVPROC glGetMultisamplefv = 0x0;
	PFNGLSAMPLEMASKIPROC glSampleMaski = 0x0;
	PFNGLGETTEXLEVELPARAMETERIVPROC glGetTexLevelParameteriv = 0x0;
	PFNGLGETTEXLEVELPARAMETERFVPROC glGetTexLevelParameterfv = 0x0;
	PFNGLBINDVERTEXBUFFERPROC glBindVertexBuffer = 0x0;
	PFNGLVERTEXATTRIBFORMATPROC glVertexAttribFormat = 0x0;
	PFNGLVERTEXATTRIBIFORMATPROC glVertexAttribIFormat = 0x0;
	PFNGLVERTEXATTRIBBINDINGPROC glVertexAttribBinding = 0x0;
	PFNGLVERTEXBINDINGDIVISORPROC glVertexBindingDivisor = 0x0;

	// Core ES 3.2
	PFNGLPATCHPARAMETERIPROC glPatchParameteri = 0x0;
	PFNGLBLENDBARRIERPROC glBlendBarrier = 0x0;
	PFNGLCOPYIMAGESUBDATAPROC glCopyImageSubData = 0x0;
	PFNGLDEBUGMESSAGECONTROLPROC glDebugMessageControl = 0x0;
	PFNGLDEBUGMESSAGEINSERTPROC glDebugMessageInsert = 0x0;
	PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback = 0x0;
	PFNGLGETDEBUGMESSAGELOGPROC glGetDebugMessageLog = 0x0;
	PFNGLPUSHDEBUGGROUPPROC glPushDebugGroup = 0x0;
	PFNGLPOPDEBUGGROUPPROC glPopDebugGroup = 0x0;
	PFNGLOBJECTLABELPROC glObjectLabel = 0x0;
	PFNGLGETOBJECTLABELPROC glGetObjectLabel = 0x0;
	PFNGLOBJECTPTRLABELPROC glObjectPtrLabel = 0x0;
	PFNGLGETOBJECTPTRLABELPROC glGetObjectPtrLabel = 0x0;
	PFNGLGETPOINTERVPROC glGetPointerv = 0x0;
	PFNGLENABLEIPROC glEnablei = 0x0;
	PFNGLDISABLEIPROC glDisablei = 0x0;
	PFNGLBLENDEQUATIONIPROC glBlendEquationi = 0x0;
	PFNGLBLENDEQUATIONSEPARATEIPROC glBlendEquationSeparatei = 0x0;
	PFNGLBLENDFUNCIPROC glBlendFunci = 0x0;
	PFNGLBLENDFUNCSEPARATEIPROC glBlendFuncSeparatei = 0x0;
	PFNGLCOLORMASKIPROC glColorMaski = 0x0;
	PFNGLISENABLEDIPROC glIsEnabledi = 0x0;
	PFNGLDRAWELEMENTSBASEVERTEXPROC glDrawElementsBaseVertex = 0x0;
	PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC glDrawRangeElementsBaseVertex = 0x0;
	PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glDrawElementsInstancedBaseVertex = 0x0;
	PFNGLFRAMEBUFFERTEXTUREPROC glFramebufferTexture = 0x0;
	PFNGLPRIMITIVEBOUNDINGBOXPROC glPrimitiveBoundingBox = 0x0;
	PFNGLGETGRAPHICSRESETSTATUSPROC glGetGraphicsResetStatus = 0x0;
	PFNGLREADNPIXELSPROC glReadnPixels = 0x0;
	PFNGLGETNUNIFORMFVPROC glGetnUniformfv = 0x0;
	PFNGLGETNUNIFORMIVPROC glGetnUniformiv = 0x0;
	PFNGLGETNUNIFORMUIVPROC glGetnUniformuiv = 0x0;
	PFNGLMINSAMPLESHADINGPROC glMinSampleShading = 0x0;
	PFNGLTEXPARAMETERIIVPROC glTexParameterIiv = 0x0;
	PFNGLTEXPARAMETERIUIVPROC glTexParameterIuiv = 0x0;
	PFNGLGETTEXPARAMETERIIVPROC glGetTexParameterIiv = 0x0;
	PFNGLGETTEXPARAMETERIUIVPROC glGetTexParameterIuiv = 0x0;
	PFNGLSAMPLERPARAMETERIIVPROC glSamplerParameterIiv = 0x0;
	PFNGLSAMPLERPARAMETERIUIVPROC glSamplerParameterIuiv = 0x0;
	PFNGLGETSAMPLERPARAMETERIIVPROC glGetSamplerParameterIiv = 0x0;
	PFNGLGETSAMPLERPARAMETERIUIVPROC glGetSamplerParameterIuiv = 0x0;
	PFNGLTEXBUFFERPROC glTexBuffer = 0x0;
	PFNGLTEXBUFFERRANGEPROC glTexBufferRange = 0x0;
	PFNGLTEXSTORAGE3DMULTISAMPLEPROC glTexStorage3DMultisample = 0x0;

}  // namespace gles


bool checkExtensionSupported( const char *extName )
{
	const char *extensions = ( char * ) glGetString( GL_EXTENSIONS );

	// GL core profile (desktop) devolve NULL aqui - so' o indice serve
	if( !extensions )
	{
		GLint numExtensions = 0;
		glGetIntegerv( GL_NUM_EXTENSIONS, &numExtensions );
		for( GLint i = 0; i < numExtensions; ++i )
		{
			const char *ext = ( const char * ) glGetStringi( GL_EXTENSIONS, ( GLuint ) i );
			if( ext && strcmp( ext, extName ) == 0 ) return true;
		}
		return false;
	}

	size_t nameLen = strlen( extName );
	const char *pos;
	while ( ( pos = strstr( extensions, extName ) ) != 0x0 )
	{
		char c = pos[ nameLen ];
		if ( c == ' ' || c == '\0' ) return true;
		extensions = pos + nameLen;
	}

	return false;
}


void getOpenGLESVersion()
{
	const char *version = ( const char * ) glGetString( GL_VERSION );
	if ( !version ) return;

	const char *dot = strchr( version, '.' );
	if ( !dot ) return;

	const char *start = dot;
	while ( start > version && start[-1] >= '0' && start[-1] <= '9' ) --start;

	glESExt::majorVersion = atoi( start );
	glESExt::minorVersion = atoi( dot + 1 );

	// A desktop "4.6" is not ES 4.6: its core has no glBlendBarrier or
	// glPrimitiveBoundingBox, so the 3.1/3.2 blocks would fail the load.
	if( !strstr( version, "OpenGL ES" ) && glESExt::majorVersion * 10 + glESExt::minorVersion > 30 )
	{
		glESExt::majorVersion = 3;
		glESExt::minorVersion = 0;
	}
}

static void *( *g_proc_loader )( const char * ) = 0x0;

void glesSetProcAddressLoader( void *( *fn )( const char * ) )
{
	g_proc_loader = fn;
}

void *platformGetProcAddress( const char *funcName )
{
	if( g_proc_loader )
	{
		void *p = g_proc_loader( funcName );
		if( p ) return p;
	}

#if defined( GLES3_LOADER_ONLY )
	( void ) funcName;
	return 0x0;
#elif defined( __EMSCRIPTEN__ )
	// Web (WebGL 1/2 -> ES 2.0/3.0): wasm não tem dlsym, fica só o EGL.
	// Ligar com -sGL_ENABLE_GET_PROC_ADDRESS=1 (senão devolve sempre NULL).
	return ( void * ) eglGetProcAddress( funcName );
#elif defined( __APPLE__ )
	// iOS: OpenGLES.framework está linkado -> dlsym(RTLD_DEFAULT)
	return dlsym( RTLD_DEFAULT, funcName );
#else
	// Windows (ANGLE), Linux e Android -> EGL; fallback dlsym
	void *p = ( void * ) eglGetProcAddress( funcName );
#if !defined( _WIN32 ) && !defined( _WIN32_WCE )
	if( !p ) p = dlsym( RTLD_DEFAULT, funcName );
#endif
	return p;
#endif
}

// A mesma função pode existir com sufixo EXT/OES ou sem sufixo (core): tenta por ordem.
static void *platformGetProcAddressAny( const char *n1, const char *n2 = 0x0, const char *n3 = 0x0 )
{
	void *p = platformGetProcAddress( n1 );
	if( !p && n2 ) p = platformGetProcAddress( n2 );
	if( !p && n3 ) p = platformGetProcAddress( n3 );
	return p;
}

bool initOpenGLExtensions()
{
	bool r = true;

	// o parse da versão e o teste de extensões precisam destas antes de tudo
	glGetString = ( PFNGLGETSTRINGPROC ) platformGetProcAddress( "glGetString" );
	if( !glGetString ) return false;

	getOpenGLESVersion();

	// Core ES 2.0
	if ( glESExt::majorVersion * 10 + glESExt::minorVersion >= 20 )
	{
		r &= ( glActiveTexture = ( PFNGLACTIVETEXTUREPROC ) platformGetProcAddress( "glActiveTexture" ) ) != 0x0;
		r &= ( glAttachShader = ( PFNGLATTACHSHADERPROC ) platformGetProcAddress( "glAttachShader" ) ) != 0x0;
		r &= ( glBindAttribLocation = ( PFNGLBINDATTRIBLOCATIONPROC ) platformGetProcAddress( "glBindAttribLocation" ) ) != 0x0;
		r &= ( glBindBuffer = ( PFNGLBINDBUFFERPROC ) platformGetProcAddress( "glBindBuffer" ) ) != 0x0;
		r &= ( glBindFramebuffer = ( PFNGLBINDFRAMEBUFFERPROC ) platformGetProcAddress( "glBindFramebuffer" ) ) != 0x0;
		r &= ( glBindRenderbuffer = ( PFNGLBINDRENDERBUFFERPROC ) platformGetProcAddress( "glBindRenderbuffer" ) ) != 0x0;
		r &= ( glBindTexture = ( PFNGLBINDTEXTUREPROC ) platformGetProcAddress( "glBindTexture" ) ) != 0x0;
		r &= ( glBlendColor = ( PFNGLBLENDCOLORPROC ) platformGetProcAddress( "glBlendColor" ) ) != 0x0;
		r &= ( glBlendEquation = ( PFNGLBLENDEQUATIONPROC ) platformGetProcAddress( "glBlendEquation" ) ) != 0x0;
		r &= ( glBlendEquationSeparate = ( PFNGLBLENDEQUATIONSEPARATEPROC ) platformGetProcAddress( "glBlendEquationSeparate" ) ) != 0x0;
		r &= ( glBlendFunc = ( PFNGLBLENDFUNCPROC ) platformGetProcAddress( "glBlendFunc" ) ) != 0x0;
		r &= ( glBlendFuncSeparate = ( PFNGLBLENDFUNCSEPARATEPROC ) platformGetProcAddress( "glBlendFuncSeparate" ) ) != 0x0;
		r &= ( glBufferData = ( PFNGLBUFFERDATAPROC ) platformGetProcAddress( "glBufferData" ) ) != 0x0;
		r &= ( glBufferSubData = ( PFNGLBUFFERSUBDATAPROC ) platformGetProcAddress( "glBufferSubData" ) ) != 0x0;
		r &= ( glCheckFramebufferStatus = ( PFNGLCHECKFRAMEBUFFERSTATUSPROC ) platformGetProcAddress( "glCheckFramebufferStatus" ) ) != 0x0;
		r &= ( glClear = ( PFNGLCLEARPROC ) platformGetProcAddress( "glClear" ) ) != 0x0;
		r &= ( glClearColor = ( PFNGLCLEARCOLORPROC ) platformGetProcAddress( "glClearColor" ) ) != 0x0;
		r &= ( glClearDepthf = ( PFNGLCLEARDEPTHFPROC ) platformGetProcAddress( "glClearDepthf" ) ) != 0x0;
		r &= ( glClearStencil = ( PFNGLCLEARSTENCILPROC ) platformGetProcAddress( "glClearStencil" ) ) != 0x0;
		r &= ( glColorMask = ( PFNGLCOLORMASKPROC ) platformGetProcAddress( "glColorMask" ) ) != 0x0;
		r &= ( glCompileShader = ( PFNGLCOMPILESHADERPROC ) platformGetProcAddress( "glCompileShader" ) ) != 0x0;
		r &= ( glCompressedTexImage2D = ( PFNGLCOMPRESSEDTEXIMAGE2DPROC ) platformGetProcAddress( "glCompressedTexImage2D" ) ) != 0x0;
		r &= ( glCompressedTexSubImage2D = ( PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC ) platformGetProcAddress( "glCompressedTexSubImage2D" ) ) != 0x0;
		r &= ( glCopyTexImage2D = ( PFNGLCOPYTEXIMAGE2DPROC ) platformGetProcAddress( "glCopyTexImage2D" ) ) != 0x0;
		r &= ( glCopyTexSubImage2D = ( PFNGLCOPYTEXSUBIMAGE2DPROC ) platformGetProcAddress( "glCopyTexSubImage2D" ) ) != 0x0;
		r &= ( glCreateProgram = ( PFNGLCREATEPROGRAMPROC ) platformGetProcAddress( "glCreateProgram" ) ) != 0x0;
		r &= ( glCreateShader = ( PFNGLCREATESHADERPROC ) platformGetProcAddress( "glCreateShader" ) ) != 0x0;
		r &= ( glCullFace = ( PFNGLCULLFACEPROC ) platformGetProcAddress( "glCullFace" ) ) != 0x0;
		r &= ( glDeleteBuffers = ( PFNGLDELETEBUFFERSPROC ) platformGetProcAddress( "glDeleteBuffers" ) ) != 0x0;
		r &= ( glDeleteFramebuffers = ( PFNGLDELETEFRAMEBUFFERSPROC ) platformGetProcAddress( "glDeleteFramebuffers" ) ) != 0x0;
		r &= ( glDeleteProgram = ( PFNGLDELETEPROGRAMPROC ) platformGetProcAddress( "glDeleteProgram" ) ) != 0x0;
		r &= ( glDeleteRenderbuffers = ( PFNGLDELETERENDERBUFFERSPROC ) platformGetProcAddress( "glDeleteRenderbuffers" ) ) != 0x0;
		r &= ( glDeleteShader = ( PFNGLDELETESHADERPROC ) platformGetProcAddress( "glDeleteShader" ) ) != 0x0;
		r &= ( glDeleteTextures = ( PFNGLDELETETEXTURESPROC ) platformGetProcAddress( "glDeleteTextures" ) ) != 0x0;
		r &= ( glDepthFunc = ( PFNGLDEPTHFUNCPROC ) platformGetProcAddress( "glDepthFunc" ) ) != 0x0;
		r &= ( glDepthMask = ( PFNGLDEPTHMASKPROC ) platformGetProcAddress( "glDepthMask" ) ) != 0x0;
		r &= ( glDepthRangef = ( PFNGLDEPTHRANGEFPROC ) platformGetProcAddress( "glDepthRangef" ) ) != 0x0;
		r &= ( glDetachShader = ( PFNGLDETACHSHADERPROC ) platformGetProcAddress( "glDetachShader" ) ) != 0x0;
		r &= ( glDisable = ( PFNGLDISABLEPROC ) platformGetProcAddress( "glDisable" ) ) != 0x0;
		r &= ( glDisableVertexAttribArray = ( PFNGLDISABLEVERTEXATTRIBARRAYPROC ) platformGetProcAddress( "glDisableVertexAttribArray" ) ) != 0x0;
		r &= ( glDrawArrays = ( PFNGLDRAWARRAYSPROC ) platformGetProcAddress( "glDrawArrays" ) ) != 0x0;
		r &= ( glDrawElements = ( PFNGLDRAWELEMENTSPROC ) platformGetProcAddress( "glDrawElements" ) ) != 0x0;
		r &= ( glEnable = ( PFNGLENABLEPROC ) platformGetProcAddress( "glEnable" ) ) != 0x0;
		r &= ( glEnableVertexAttribArray = ( PFNGLENABLEVERTEXATTRIBARRAYPROC ) platformGetProcAddress( "glEnableVertexAttribArray" ) ) != 0x0;
		r &= ( glFinish = ( PFNGLFINISHPROC ) platformGetProcAddress( "glFinish" ) ) != 0x0;
		r &= ( glFlush = ( PFNGLFLUSHPROC ) platformGetProcAddress( "glFlush" ) ) != 0x0;
		r &= ( glFramebufferRenderbuffer = ( PFNGLFRAMEBUFFERRENDERBUFFERPROC ) platformGetProcAddress( "glFramebufferRenderbuffer" ) ) != 0x0;
		r &= ( glFramebufferTexture2D = ( PFNGLFRAMEBUFFERTEXTURE2DPROC ) platformGetProcAddress( "glFramebufferTexture2D" ) ) != 0x0;
		r &= ( glFrontFace = ( PFNGLFRONTFACEPROC ) platformGetProcAddress( "glFrontFace" ) ) != 0x0;
		r &= ( glGenBuffers = ( PFNGLGENBUFFERSPROC ) platformGetProcAddress( "glGenBuffers" ) ) != 0x0;
		r &= ( glGenerateMipmap = ( PFNGLGENERATEMIPMAPPROC ) platformGetProcAddress( "glGenerateMipmap" ) ) != 0x0;
		r &= ( glGenFramebuffers = ( PFNGLGENFRAMEBUFFERSPROC ) platformGetProcAddress( "glGenFramebuffers" ) ) != 0x0;
		r &= ( glGenRenderbuffers = ( PFNGLGENRENDERBUFFERSPROC ) platformGetProcAddress( "glGenRenderbuffers" ) ) != 0x0;
		r &= ( glGenTextures = ( PFNGLGENTEXTURESPROC ) platformGetProcAddress( "glGenTextures" ) ) != 0x0;
		r &= ( glGetActiveAttrib = ( PFNGLGETACTIVEATTRIBPROC ) platformGetProcAddress( "glGetActiveAttrib" ) ) != 0x0;
		r &= ( glGetActiveUniform = ( PFNGLGETACTIVEUNIFORMPROC ) platformGetProcAddress( "glGetActiveUniform" ) ) != 0x0;
		r &= ( glGetAttachedShaders = ( PFNGLGETATTACHEDSHADERSPROC ) platformGetProcAddress( "glGetAttachedShaders" ) ) != 0x0;
		r &= ( glGetAttribLocation = ( PFNGLGETATTRIBLOCATIONPROC ) platformGetProcAddress( "glGetAttribLocation" ) ) != 0x0;
		r &= ( glGetBooleanv = ( PFNGLGETBOOLEANVPROC ) platformGetProcAddress( "glGetBooleanv" ) ) != 0x0;
		r &= ( glGetBufferParameteriv = ( PFNGLGETBUFFERPARAMETERIVPROC ) platformGetProcAddress( "glGetBufferParameteriv" ) ) != 0x0;
		r &= ( glGetError = ( PFNGLGETERRORPROC ) platformGetProcAddress( "glGetError" ) ) != 0x0;
		r &= ( glGetFloatv = ( PFNGLGETFLOATVPROC ) platformGetProcAddress( "glGetFloatv" ) ) != 0x0;
		r &= ( glGetFramebufferAttachmentParameteriv = ( PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC ) platformGetProcAddress( "glGetFramebufferAttachmentParameteriv" ) ) != 0x0;
		r &= ( glGetIntegerv = ( PFNGLGETINTEGERVPROC ) platformGetProcAddress( "glGetIntegerv" ) ) != 0x0;
		r &= ( glGetProgramiv = ( PFNGLGETPROGRAMIVPROC ) platformGetProcAddress( "glGetProgramiv" ) ) != 0x0;
		r &= ( glGetProgramInfoLog = ( PFNGLGETPROGRAMINFOLOGPROC ) platformGetProcAddress( "glGetProgramInfoLog" ) ) != 0x0;
		r &= ( glGetRenderbufferParameteriv = ( PFNGLGETRENDERBUFFERPARAMETERIVPROC ) platformGetProcAddress( "glGetRenderbufferParameteriv" ) ) != 0x0;
		r &= ( glGetShaderiv = ( PFNGLGETSHADERIVPROC ) platformGetProcAddress( "glGetShaderiv" ) ) != 0x0;
		r &= ( glGetShaderInfoLog = ( PFNGLGETSHADERINFOLOGPROC ) platformGetProcAddress( "glGetShaderInfoLog" ) ) != 0x0;
		r &= ( glGetShaderPrecisionFormat = ( PFNGLGETSHADERPRECISIONFORMATPROC ) platformGetProcAddress( "glGetShaderPrecisionFormat" ) ) != 0x0;
		r &= ( glGetShaderSource = ( PFNGLGETSHADERSOURCEPROC ) platformGetProcAddress( "glGetShaderSource" ) ) != 0x0;
		r &= ( glGetString = ( PFNGLGETSTRINGPROC ) platformGetProcAddress( "glGetString" ) ) != 0x0;
		r &= ( glGetTexParameterfv = ( PFNGLGETTEXPARAMETERFVPROC ) platformGetProcAddress( "glGetTexParameterfv" ) ) != 0x0;
		r &= ( glGetTexParameteriv = ( PFNGLGETTEXPARAMETERIVPROC ) platformGetProcAddress( "glGetTexParameteriv" ) ) != 0x0;
		r &= ( glGetUniformfv = ( PFNGLGETUNIFORMFVPROC ) platformGetProcAddress( "glGetUniformfv" ) ) != 0x0;
		r &= ( glGetUniformiv = ( PFNGLGETUNIFORMIVPROC ) platformGetProcAddress( "glGetUniformiv" ) ) != 0x0;
		r &= ( glGetUniformLocation = ( PFNGLGETUNIFORMLOCATIONPROC ) platformGetProcAddress( "glGetUniformLocation" ) ) != 0x0;
		r &= ( glGetVertexAttribfv = ( PFNGLGETVERTEXATTRIBFVPROC ) platformGetProcAddress( "glGetVertexAttribfv" ) ) != 0x0;
		r &= ( glGetVertexAttribiv = ( PFNGLGETVERTEXATTRIBIVPROC ) platformGetProcAddress( "glGetVertexAttribiv" ) ) != 0x0;
		r &= ( glGetVertexAttribPointerv = ( PFNGLGETVERTEXATTRIBPOINTERVPROC ) platformGetProcAddress( "glGetVertexAttribPointerv" ) ) != 0x0;
		r &= ( glHint = ( PFNGLHINTPROC ) platformGetProcAddress( "glHint" ) ) != 0x0;
		r &= ( glIsBuffer = ( PFNGLISBUFFERPROC ) platformGetProcAddress( "glIsBuffer" ) ) != 0x0;
		r &= ( glIsEnabled = ( PFNGLISENABLEDPROC ) platformGetProcAddress( "glIsEnabled" ) ) != 0x0;
		r &= ( glIsFramebuffer = ( PFNGLISFRAMEBUFFERPROC ) platformGetProcAddress( "glIsFramebuffer" ) ) != 0x0;
		r &= ( glIsProgram = ( PFNGLISPROGRAMPROC ) platformGetProcAddress( "glIsProgram" ) ) != 0x0;
		r &= ( glIsRenderbuffer = ( PFNGLISRENDERBUFFERPROC ) platformGetProcAddress( "glIsRenderbuffer" ) ) != 0x0;
		r &= ( glIsShader = ( PFNGLISSHADERPROC ) platformGetProcAddress( "glIsShader" ) ) != 0x0;
		r &= ( glIsTexture = ( PFNGLISTEXTUREPROC ) platformGetProcAddress( "glIsTexture" ) ) != 0x0;
		r &= ( glLineWidth = ( PFNGLLINEWIDTHPROC ) platformGetProcAddress( "glLineWidth" ) ) != 0x0;
		r &= ( glLinkProgram = ( PFNGLLINKPROGRAMPROC ) platformGetProcAddress( "glLinkProgram" ) ) != 0x0;
		r &= ( glPixelStorei = ( PFNGLPIXELSTOREIPROC ) platformGetProcAddress( "glPixelStorei" ) ) != 0x0;
		r &= ( glPolygonOffset = ( PFNGLPOLYGONOFFSETPROC ) platformGetProcAddress( "glPolygonOffset" ) ) != 0x0;
		r &= ( glReadPixels = ( PFNGLREADPIXELSPROC ) platformGetProcAddress( "glReadPixels" ) ) != 0x0;
		r &= ( glReleaseShaderCompiler = ( PFNGLRELEASESHADERCOMPILERPROC ) platformGetProcAddress( "glReleaseShaderCompiler" ) ) != 0x0;
		r &= ( glRenderbufferStorage = ( PFNGLRENDERBUFFERSTORAGEPROC ) platformGetProcAddress( "glRenderbufferStorage" ) ) != 0x0;
		r &= ( glSampleCoverage = ( PFNGLSAMPLECOVERAGEPROC ) platformGetProcAddress( "glSampleCoverage" ) ) != 0x0;
		r &= ( glScissor = ( PFNGLSCISSORPROC ) platformGetProcAddress( "glScissor" ) ) != 0x0;
		r &= ( glShaderBinary = ( PFNGLSHADERBINARYPROC ) platformGetProcAddress( "glShaderBinary" ) ) != 0x0;
		r &= ( glShaderSource = ( PFNGLSHADERSOURCEPROC ) platformGetProcAddress( "glShaderSource" ) ) != 0x0;
		r &= ( glStencilFunc = ( PFNGLSTENCILFUNCPROC ) platformGetProcAddress( "glStencilFunc" ) ) != 0x0;
		r &= ( glStencilFuncSeparate = ( PFNGLSTENCILFUNCSEPARATEPROC ) platformGetProcAddress( "glStencilFuncSeparate" ) ) != 0x0;
		r &= ( glStencilMask = ( PFNGLSTENCILMASKPROC ) platformGetProcAddress( "glStencilMask" ) ) != 0x0;
		r &= ( glStencilMaskSeparate = ( PFNGLSTENCILMASKSEPARATEPROC ) platformGetProcAddress( "glStencilMaskSeparate" ) ) != 0x0;
		r &= ( glStencilOp = ( PFNGLSTENCILOPPROC ) platformGetProcAddress( "glStencilOp" ) ) != 0x0;
		r &= ( glStencilOpSeparate = ( PFNGLSTENCILOPSEPARATEPROC ) platformGetProcAddress( "glStencilOpSeparate" ) ) != 0x0;
		r &= ( glTexImage2D = ( PFNGLTEXIMAGE2DPROC ) platformGetProcAddress( "glTexImage2D" ) ) != 0x0;
		r &= ( glTexParameterf = ( PFNGLTEXPARAMETERFPROC ) platformGetProcAddress( "glTexParameterf" ) ) != 0x0;
		r &= ( glTexParameterfv = ( PFNGLTEXPARAMETERFVPROC ) platformGetProcAddress( "glTexParameterfv" ) ) != 0x0;
		r &= ( glTexParameteri = ( PFNGLTEXPARAMETERIPROC ) platformGetProcAddress( "glTexParameteri" ) ) != 0x0;
		r &= ( glTexParameteriv = ( PFNGLTEXPARAMETERIVPROC ) platformGetProcAddress( "glTexParameteriv" ) ) != 0x0;
		r &= ( glTexSubImage2D = ( PFNGLTEXSUBIMAGE2DPROC ) platformGetProcAddress( "glTexSubImage2D" ) ) != 0x0;
		r &= ( glUniform1f = ( PFNGLUNIFORM1FPROC ) platformGetProcAddress( "glUniform1f" ) ) != 0x0;
		r &= ( glUniform1fv = ( PFNGLUNIFORM1FVPROC ) platformGetProcAddress( "glUniform1fv" ) ) != 0x0;
		r &= ( glUniform1i = ( PFNGLUNIFORM1IPROC ) platformGetProcAddress( "glUniform1i" ) ) != 0x0;
		r &= ( glUniform1iv = ( PFNGLUNIFORM1IVPROC ) platformGetProcAddress( "glUniform1iv" ) ) != 0x0;
		r &= ( glUniform2f = ( PFNGLUNIFORM2FPROC ) platformGetProcAddress( "glUniform2f" ) ) != 0x0;
		r &= ( glUniform2fv = ( PFNGLUNIFORM2FVPROC ) platformGetProcAddress( "glUniform2fv" ) ) != 0x0;
		r &= ( glUniform2i = ( PFNGLUNIFORM2IPROC ) platformGetProcAddress( "glUniform2i" ) ) != 0x0;
		r &= ( glUniform2iv = ( PFNGLUNIFORM2IVPROC ) platformGetProcAddress( "glUniform2iv" ) ) != 0x0;
		r &= ( glUniform3f = ( PFNGLUNIFORM3FPROC ) platformGetProcAddress( "glUniform3f" ) ) != 0x0;
		r &= ( glUniform3fv = ( PFNGLUNIFORM3FVPROC ) platformGetProcAddress( "glUniform3fv" ) ) != 0x0;
		r &= ( glUniform3i = ( PFNGLUNIFORM3IPROC ) platformGetProcAddress( "glUniform3i" ) ) != 0x0;
		r &= ( glUniform3iv = ( PFNGLUNIFORM3IVPROC ) platformGetProcAddress( "glUniform3iv" ) ) != 0x0;
		r &= ( glUniform4f = ( PFNGLUNIFORM4FPROC ) platformGetProcAddress( "glUniform4f" ) ) != 0x0;
		r &= ( glUniform4fv = ( PFNGLUNIFORM4FVPROC ) platformGetProcAddress( "glUniform4fv" ) ) != 0x0;
		r &= ( glUniform4i = ( PFNGLUNIFORM4IPROC ) platformGetProcAddress( "glUniform4i" ) ) != 0x0;
		r &= ( glUniform4iv = ( PFNGLUNIFORM4IVPROC ) platformGetProcAddress( "glUniform4iv" ) ) != 0x0;
		r &= ( glUniformMatrix2fv = ( PFNGLUNIFORMMATRIX2FVPROC ) platformGetProcAddress( "glUniformMatrix2fv" ) ) != 0x0;
		r &= ( glUniformMatrix3fv = ( PFNGLUNIFORMMATRIX3FVPROC ) platformGetProcAddress( "glUniformMatrix3fv" ) ) != 0x0;
		r &= ( glUniformMatrix4fv = ( PFNGLUNIFORMMATRIX4FVPROC ) platformGetProcAddress( "glUniformMatrix4fv" ) ) != 0x0;
		r &= ( glUseProgram = ( PFNGLUSEPROGRAMPROC ) platformGetProcAddress( "glUseProgram" ) ) != 0x0;
		r &= ( glValidateProgram = ( PFNGLVALIDATEPROGRAMPROC ) platformGetProcAddress( "glValidateProgram" ) ) != 0x0;
		r &= ( glVertexAttrib1f = ( PFNGLVERTEXATTRIB1FPROC ) platformGetProcAddress( "glVertexAttrib1f" ) ) != 0x0;
		r &= ( glVertexAttrib1fv = ( PFNGLVERTEXATTRIB1FVPROC ) platformGetProcAddress( "glVertexAttrib1fv" ) ) != 0x0;
		r &= ( glVertexAttrib2f = ( PFNGLVERTEXATTRIB2FPROC ) platformGetProcAddress( "glVertexAttrib2f" ) ) != 0x0;
		r &= ( glVertexAttrib2fv = ( PFNGLVERTEXATTRIB2FVPROC ) platformGetProcAddress( "glVertexAttrib2fv" ) ) != 0x0;
		r &= ( glVertexAttrib3f = ( PFNGLVERTEXATTRIB3FPROC ) platformGetProcAddress( "glVertexAttrib3f" ) ) != 0x0;
		r &= ( glVertexAttrib3fv = ( PFNGLVERTEXATTRIB3FVPROC ) platformGetProcAddress( "glVertexAttrib3fv" ) ) != 0x0;
		r &= ( glVertexAttrib4f = ( PFNGLVERTEXATTRIB4FPROC ) platformGetProcAddress( "glVertexAttrib4f" ) ) != 0x0;
		r &= ( glVertexAttrib4fv = ( PFNGLVERTEXATTRIB4FVPROC ) platformGetProcAddress( "glVertexAttrib4fv" ) ) != 0x0;
		r &= ( glVertexAttribPointer = ( PFNGLVERTEXATTRIBPOINTERPROC ) platformGetProcAddress( "glVertexAttribPointer" ) ) != 0x0;
		r &= ( glViewport = ( PFNGLVIEWPORTPROC ) platformGetProcAddress( "glViewport" ) ) != 0x0;
	}

	// Core ES 3.0
	if ( glESExt::majorVersion * 10 + glESExt::minorVersion >= 30 )
	{
		r &= ( glReadBuffer = ( PFNGLREADBUFFERPROC ) platformGetProcAddress( "glReadBuffer" ) ) != 0x0;
		r &= ( glDrawRangeElements = ( PFNGLDRAWRANGEELEMENTSPROC ) platformGetProcAddress( "glDrawRangeElements" ) ) != 0x0;
		r &= ( glTexImage3D = ( PFNGLTEXIMAGE3DPROC ) platformGetProcAddress( "glTexImage3D" ) ) != 0x0;
		r &= ( glTexSubImage3D = ( PFNGLTEXSUBIMAGE3DPROC ) platformGetProcAddress( "glTexSubImage3D" ) ) != 0x0;
		r &= ( glCopyTexSubImage3D = ( PFNGLCOPYTEXSUBIMAGE3DPROC ) platformGetProcAddress( "glCopyTexSubImage3D" ) ) != 0x0;
		r &= ( glCompressedTexImage3D = ( PFNGLCOMPRESSEDTEXIMAGE3DPROC ) platformGetProcAddress( "glCompressedTexImage3D" ) ) != 0x0;
		r &= ( glCompressedTexSubImage3D = ( PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC ) platformGetProcAddress( "glCompressedTexSubImage3D" ) ) != 0x0;
		r &= ( glGenQueries = ( PFNGLGENQUERIESPROC ) platformGetProcAddress( "glGenQueries" ) ) != 0x0;
		r &= ( glDeleteQueries = ( PFNGLDELETEQUERIESPROC ) platformGetProcAddress( "glDeleteQueries" ) ) != 0x0;
		r &= ( glIsQuery = ( PFNGLISQUERYPROC ) platformGetProcAddress( "glIsQuery" ) ) != 0x0;
		r &= ( glBeginQuery = ( PFNGLBEGINQUERYPROC ) platformGetProcAddress( "glBeginQuery" ) ) != 0x0;
		r &= ( glEndQuery = ( PFNGLENDQUERYPROC ) platformGetProcAddress( "glEndQuery" ) ) != 0x0;
		r &= ( glGetQueryiv = ( PFNGLGETQUERYIVPROC ) platformGetProcAddress( "glGetQueryiv" ) ) != 0x0;
		r &= ( glGetQueryObjectuiv = ( PFNGLGETQUERYOBJECTUIVPROC ) platformGetProcAddress( "glGetQueryObjectuiv" ) ) != 0x0;
		// O WebGL2 não expõe o buffer mapping do ES 3.0 -> no web são best-effort.
		glUnmapBuffer = ( PFNGLUNMAPBUFFERPROC ) platformGetProcAddress( "glUnmapBuffer" );
		glGetBufferPointerv = ( PFNGLGETBUFFERPOINTERVPROC ) platformGetProcAddress( "glGetBufferPointerv" );
#if !defined( __EMSCRIPTEN__ )
		r &= glUnmapBuffer != 0x0 && glGetBufferPointerv != 0x0;
#endif
		r &= ( glDrawBuffers = ( PFNGLDRAWBUFFERSPROC ) platformGetProcAddress( "glDrawBuffers" ) ) != 0x0;
		r &= ( glUniformMatrix2x3fv = ( PFNGLUNIFORMMATRIX2X3FVPROC ) platformGetProcAddress( "glUniformMatrix2x3fv" ) ) != 0x0;
		r &= ( glUniformMatrix3x2fv = ( PFNGLUNIFORMMATRIX3X2FVPROC ) platformGetProcAddress( "glUniformMatrix3x2fv" ) ) != 0x0;
		r &= ( glUniformMatrix2x4fv = ( PFNGLUNIFORMMATRIX2X4FVPROC ) platformGetProcAddress( "glUniformMatrix2x4fv" ) ) != 0x0;
		r &= ( glUniformMatrix4x2fv = ( PFNGLUNIFORMMATRIX4X2FVPROC ) platformGetProcAddress( "glUniformMatrix4x2fv" ) ) != 0x0;
		r &= ( glUniformMatrix3x4fv = ( PFNGLUNIFORMMATRIX3X4FVPROC ) platformGetProcAddress( "glUniformMatrix3x4fv" ) ) != 0x0;
		r &= ( glUniformMatrix4x3fv = ( PFNGLUNIFORMMATRIX4X3FVPROC ) platformGetProcAddress( "glUniformMatrix4x3fv" ) ) != 0x0;
		r &= ( glBlitFramebuffer = ( PFNGLBLITFRAMEBUFFERPROC ) platformGetProcAddress( "glBlitFramebuffer" ) ) != 0x0;
		r &= ( glRenderbufferStorageMultisample = ( PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC ) platformGetProcAddress( "glRenderbufferStorageMultisample" ) ) != 0x0;
		r &= ( glFramebufferTextureLayer = ( PFNGLFRAMEBUFFERTEXTURELAYERPROC ) platformGetProcAddress( "glFramebufferTextureLayer" ) ) != 0x0;
		// O WebGL2 não expõe o buffer mapping do ES 3.0 -> no web são best-effort.
		glMapBufferRange = ( PFNGLMAPBUFFERRANGEPROC ) platformGetProcAddress( "glMapBufferRange" );
		glFlushMappedBufferRange = ( PFNGLFLUSHMAPPEDBUFFERRANGEPROC ) platformGetProcAddress( "glFlushMappedBufferRange" );
#if !defined( __EMSCRIPTEN__ )
		r &= glMapBufferRange != 0x0 && glFlushMappedBufferRange != 0x0;
#endif
		r &= ( glBindVertexArray = ( PFNGLBINDVERTEXARRAYPROC ) platformGetProcAddress( "glBindVertexArray" ) ) != 0x0;
		r &= ( glDeleteVertexArrays = ( PFNGLDELETEVERTEXARRAYSPROC ) platformGetProcAddress( "glDeleteVertexArrays" ) ) != 0x0;
		r &= ( glGenVertexArrays = ( PFNGLGENVERTEXARRAYSPROC ) platformGetProcAddress( "glGenVertexArrays" ) ) != 0x0;
		r &= ( glIsVertexArray = ( PFNGLISVERTEXARRAYPROC ) platformGetProcAddress( "glIsVertexArray" ) ) != 0x0;
		r &= ( glGetIntegeri_v = ( PFNGLGETINTEGERI_VPROC ) platformGetProcAddress( "glGetIntegeri_v" ) ) != 0x0;
		r &= ( glBeginTransformFeedback = ( PFNGLBEGINTRANSFORMFEEDBACKPROC ) platformGetProcAddress( "glBeginTransformFeedback" ) ) != 0x0;
		r &= ( glEndTransformFeedback = ( PFNGLENDTRANSFORMFEEDBACKPROC ) platformGetProcAddress( "glEndTransformFeedback" ) ) != 0x0;
		r &= ( glBindBufferRange = ( PFNGLBINDBUFFERRANGEPROC ) platformGetProcAddress( "glBindBufferRange" ) ) != 0x0;
		r &= ( glBindBufferBase = ( PFNGLBINDBUFFERBASEPROC ) platformGetProcAddress( "glBindBufferBase" ) ) != 0x0;
		r &= ( glTransformFeedbackVaryings = ( PFNGLTRANSFORMFEEDBACKVARYINGSPROC ) platformGetProcAddress( "glTransformFeedbackVaryings" ) ) != 0x0;
		r &= ( glGetTransformFeedbackVarying = ( PFNGLGETTRANSFORMFEEDBACKVARYINGPROC ) platformGetProcAddress( "glGetTransformFeedbackVarying" ) ) != 0x0;
		r &= ( glVertexAttribIPointer = ( PFNGLVERTEXATTRIBIPOINTERPROC ) platformGetProcAddress( "glVertexAttribIPointer" ) ) != 0x0;
		r &= ( glGetVertexAttribIiv = ( PFNGLGETVERTEXATTRIBIIVPROC ) platformGetProcAddress( "glGetVertexAttribIiv" ) ) != 0x0;
		r &= ( glGetVertexAttribIuiv = ( PFNGLGETVERTEXATTRIBIUIVPROC ) platformGetProcAddress( "glGetVertexAttribIuiv" ) ) != 0x0;
		r &= ( glVertexAttribI4i = ( PFNGLVERTEXATTRIBI4IPROC ) platformGetProcAddress( "glVertexAttribI4i" ) ) != 0x0;
		r &= ( glVertexAttribI4ui = ( PFNGLVERTEXATTRIBI4UIPROC ) platformGetProcAddress( "glVertexAttribI4ui" ) ) != 0x0;
		r &= ( glVertexAttribI4iv = ( PFNGLVERTEXATTRIBI4IVPROC ) platformGetProcAddress( "glVertexAttribI4iv" ) ) != 0x0;
		r &= ( glVertexAttribI4uiv = ( PFNGLVERTEXATTRIBI4UIVPROC ) platformGetProcAddress( "glVertexAttribI4uiv" ) ) != 0x0;
		r &= ( glGetUniformuiv = ( PFNGLGETUNIFORMUIVPROC ) platformGetProcAddress( "glGetUniformuiv" ) ) != 0x0;
		r &= ( glGetFragDataLocation = ( PFNGLGETFRAGDATALOCATIONPROC ) platformGetProcAddress( "glGetFragDataLocation" ) ) != 0x0;
		r &= ( glUniform1ui = ( PFNGLUNIFORM1UIPROC ) platformGetProcAddress( "glUniform1ui" ) ) != 0x0;
		r &= ( glUniform2ui = ( PFNGLUNIFORM2UIPROC ) platformGetProcAddress( "glUniform2ui" ) ) != 0x0;
		r &= ( glUniform3ui = ( PFNGLUNIFORM3UIPROC ) platformGetProcAddress( "glUniform3ui" ) ) != 0x0;
		r &= ( glUniform4ui = ( PFNGLUNIFORM4UIPROC ) platformGetProcAddress( "glUniform4ui" ) ) != 0x0;
		r &= ( glUniform1uiv = ( PFNGLUNIFORM1UIVPROC ) platformGetProcAddress( "glUniform1uiv" ) ) != 0x0;
		r &= ( glUniform2uiv = ( PFNGLUNIFORM2UIVPROC ) platformGetProcAddress( "glUniform2uiv" ) ) != 0x0;
		r &= ( glUniform3uiv = ( PFNGLUNIFORM3UIVPROC ) platformGetProcAddress( "glUniform3uiv" ) ) != 0x0;
		r &= ( glUniform4uiv = ( PFNGLUNIFORM4UIVPROC ) platformGetProcAddress( "glUniform4uiv" ) ) != 0x0;
		r &= ( glClearBufferiv = ( PFNGLCLEARBUFFERIVPROC ) platformGetProcAddress( "glClearBufferiv" ) ) != 0x0;
		r &= ( glClearBufferuiv = ( PFNGLCLEARBUFFERUIVPROC ) platformGetProcAddress( "glClearBufferuiv" ) ) != 0x0;
		r &= ( glClearBufferfv = ( PFNGLCLEARBUFFERFVPROC ) platformGetProcAddress( "glClearBufferfv" ) ) != 0x0;
		r &= ( glClearBufferfi = ( PFNGLCLEARBUFFERFIPROC ) platformGetProcAddress( "glClearBufferfi" ) ) != 0x0;
		r &= ( glGetStringi = ( PFNGLGETSTRINGIPROC ) platformGetProcAddress( "glGetStringi" ) ) != 0x0;
		r &= ( glCopyBufferSubData = ( PFNGLCOPYBUFFERSUBDATAPROC ) platformGetProcAddress( "glCopyBufferSubData" ) ) != 0x0;
		r &= ( glGetUniformIndices = ( PFNGLGETUNIFORMINDICESPROC ) platformGetProcAddress( "glGetUniformIndices" ) ) != 0x0;
		r &= ( glGetActiveUniformsiv = ( PFNGLGETACTIVEUNIFORMSIVPROC ) platformGetProcAddress( "glGetActiveUniformsiv" ) ) != 0x0;
		r &= ( glGetUniformBlockIndex = ( PFNGLGETUNIFORMBLOCKINDEXPROC ) platformGetProcAddress( "glGetUniformBlockIndex" ) ) != 0x0;
		r &= ( glGetActiveUniformBlockiv = ( PFNGLGETACTIVEUNIFORMBLOCKIVPROC ) platformGetProcAddress( "glGetActiveUniformBlockiv" ) ) != 0x0;
		r &= ( glGetActiveUniformBlockName = ( PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC ) platformGetProcAddress( "glGetActiveUniformBlockName" ) ) != 0x0;
		r &= ( glUniformBlockBinding = ( PFNGLUNIFORMBLOCKBINDINGPROC ) platformGetProcAddress( "glUniformBlockBinding" ) ) != 0x0;
		r &= ( glDrawArraysInstanced = ( PFNGLDRAWARRAYSINSTANCEDPROC ) platformGetProcAddress( "glDrawArraysInstanced" ) ) != 0x0;
		r &= ( glDrawElementsInstanced = ( PFNGLDRAWELEMENTSINSTANCEDPROC ) platformGetProcAddress( "glDrawElementsInstanced" ) ) != 0x0;
		r &= ( glFenceSync = ( PFNGLFENCESYNCPROC ) platformGetProcAddress( "glFenceSync" ) ) != 0x0;
		r &= ( glIsSync = ( PFNGLISSYNCPROC ) platformGetProcAddress( "glIsSync" ) ) != 0x0;
		r &= ( glDeleteSync = ( PFNGLDELETESYNCPROC ) platformGetProcAddress( "glDeleteSync" ) ) != 0x0;
		r &= ( glClientWaitSync = ( PFNGLCLIENTWAITSYNCPROC ) platformGetProcAddress( "glClientWaitSync" ) ) != 0x0;
		r &= ( glWaitSync = ( PFNGLWAITSYNCPROC ) platformGetProcAddress( "glWaitSync" ) ) != 0x0;
		r &= ( glGetInteger64v = ( PFNGLGETINTEGER64VPROC ) platformGetProcAddress( "glGetInteger64v" ) ) != 0x0;
		r &= ( glGetSynciv = ( PFNGLGETSYNCIVPROC ) platformGetProcAddress( "glGetSynciv" ) ) != 0x0;
		r &= ( glGetInteger64i_v = ( PFNGLGETINTEGER64I_VPROC ) platformGetProcAddress( "glGetInteger64i_v" ) ) != 0x0;
		r &= ( glGetBufferParameteri64v = ( PFNGLGETBUFFERPARAMETERI64VPROC ) platformGetProcAddress( "glGetBufferParameteri64v" ) ) != 0x0;
		r &= ( glGenSamplers = ( PFNGLGENSAMPLERSPROC ) platformGetProcAddress( "glGenSamplers" ) ) != 0x0;
		r &= ( glDeleteSamplers = ( PFNGLDELETESAMPLERSPROC ) platformGetProcAddress( "glDeleteSamplers" ) ) != 0x0;
		r &= ( glIsSampler = ( PFNGLISSAMPLERPROC ) platformGetProcAddress( "glIsSampler" ) ) != 0x0;
		r &= ( glBindSampler = ( PFNGLBINDSAMPLERPROC ) platformGetProcAddress( "glBindSampler" ) ) != 0x0;
		r &= ( glSamplerParameteri = ( PFNGLSAMPLERPARAMETERIPROC ) platformGetProcAddress( "glSamplerParameteri" ) ) != 0x0;
		r &= ( glSamplerParameteriv = ( PFNGLSAMPLERPARAMETERIVPROC ) platformGetProcAddress( "glSamplerParameteriv" ) ) != 0x0;
		r &= ( glSamplerParameterf = ( PFNGLSAMPLERPARAMETERFPROC ) platformGetProcAddress( "glSamplerParameterf" ) ) != 0x0;
		r &= ( glSamplerParameterfv = ( PFNGLSAMPLERPARAMETERFVPROC ) platformGetProcAddress( "glSamplerParameterfv" ) ) != 0x0;
		r &= ( glGetSamplerParameteriv = ( PFNGLGETSAMPLERPARAMETERIVPROC ) platformGetProcAddress( "glGetSamplerParameteriv" ) ) != 0x0;
		r &= ( glGetSamplerParameterfv = ( PFNGLGETSAMPLERPARAMETERFVPROC ) platformGetProcAddress( "glGetSamplerParameterfv" ) ) != 0x0;
		r &= ( glVertexAttribDivisor = ( PFNGLVERTEXATTRIBDIVISORPROC ) platformGetProcAddress( "glVertexAttribDivisor" ) ) != 0x0;
		r &= ( glBindTransformFeedback = ( PFNGLBINDTRANSFORMFEEDBACKPROC ) platformGetProcAddress( "glBindTransformFeedback" ) ) != 0x0;
		r &= ( glDeleteTransformFeedbacks = ( PFNGLDELETETRANSFORMFEEDBACKSPROC ) platformGetProcAddress( "glDeleteTransformFeedbacks" ) ) != 0x0;
		r &= ( glGenTransformFeedbacks = ( PFNGLGENTRANSFORMFEEDBACKSPROC ) platformGetProcAddress( "glGenTransformFeedbacks" ) ) != 0x0;
		r &= ( glIsTransformFeedback = ( PFNGLISTRANSFORMFEEDBACKPROC ) platformGetProcAddress( "glIsTransformFeedback" ) ) != 0x0;
		r &= ( glPauseTransformFeedback = ( PFNGLPAUSETRANSFORMFEEDBACKPROC ) platformGetProcAddress( "glPauseTransformFeedback" ) ) != 0x0;
		r &= ( glResumeTransformFeedback = ( PFNGLRESUMETRANSFORMFEEDBACKPROC ) platformGetProcAddress( "glResumeTransformFeedback" ) ) != 0x0;
		r &= ( glGetProgramBinary = ( PFNGLGETPROGRAMBINARYPROC ) platformGetProcAddress( "glGetProgramBinary" ) ) != 0x0;
		r &= ( glProgramBinary = ( PFNGLPROGRAMBINARYPROC ) platformGetProcAddress( "glProgramBinary" ) ) != 0x0;
		r &= ( glProgramParameteri = ( PFNGLPROGRAMPARAMETERIPROC ) platformGetProcAddress( "glProgramParameteri" ) ) != 0x0;
		r &= ( glInvalidateFramebuffer = ( PFNGLINVALIDATEFRAMEBUFFERPROC ) platformGetProcAddress( "glInvalidateFramebuffer" ) ) != 0x0;
		r &= ( glInvalidateSubFramebuffer = ( PFNGLINVALIDATESUBFRAMEBUFFERPROC ) platformGetProcAddress( "glInvalidateSubFramebuffer" ) ) != 0x0;
		r &= ( glTexStorage2D = ( PFNGLTEXSTORAGE2DPROC ) platformGetProcAddress( "glTexStorage2D" ) ) != 0x0;
		r &= ( glTexStorage3D = ( PFNGLTEXSTORAGE3DPROC ) platformGetProcAddress( "glTexStorage3D" ) ) != 0x0;
		r &= ( glGetInternalformativ = ( PFNGLGETINTERNALFORMATIVPROC ) platformGetProcAddress( "glGetInternalformativ" ) ) != 0x0;
	}

	// Core ES 3.1
	if ( glESExt::majorVersion * 10 + glESExt::minorVersion >= 31 )
	{
		r &= ( glDispatchCompute = ( PFNGLDISPATCHCOMPUTEPROC ) platformGetProcAddress( "glDispatchCompute" ) ) != 0x0;
		r &= ( glGetProgramResourceIndex = ( PFNGLGETPROGRAMRESOURCEINDEXPROC ) platformGetProcAddress( "glGetProgramResourceIndex" ) ) != 0x0;
		r &= ( glGetProgramResourceiv = ( PFNGLGETPROGRAMRESOURCEIVPROC ) platformGetProcAddress( "glGetProgramResourceiv" ) ) != 0x0;
		r &= ( glMemoryBarrier = ( PFNGLMEMORYBARRIERPROC ) platformGetProcAddress( "glMemoryBarrier" ) ) != 0x0;
		r &= ( glDispatchComputeIndirect = ( PFNGLDISPATCHCOMPUTEINDIRECTPROC ) platformGetProcAddress( "glDispatchComputeIndirect" ) ) != 0x0;
		r &= ( glDrawArraysIndirect = ( PFNGLDRAWARRAYSINDIRECTPROC ) platformGetProcAddress( "glDrawArraysIndirect" ) ) != 0x0;
		r &= ( glDrawElementsIndirect = ( PFNGLDRAWELEMENTSINDIRECTPROC ) platformGetProcAddress( "glDrawElementsIndirect" ) ) != 0x0;
		r &= ( glFramebufferParameteri = ( PFNGLFRAMEBUFFERPARAMETERIPROC ) platformGetProcAddress( "glFramebufferParameteri" ) ) != 0x0;
		r &= ( glGetFramebufferParameteriv = ( PFNGLGETFRAMEBUFFERPARAMETERIVPROC ) platformGetProcAddress( "glGetFramebufferParameteriv" ) ) != 0x0;
		r &= ( glGetProgramInterfaceiv = ( PFNGLGETPROGRAMINTERFACEIVPROC ) platformGetProcAddress( "glGetProgramInterfaceiv" ) ) != 0x0;
		r &= ( glGetProgramResourceName = ( PFNGLGETPROGRAMRESOURCENAMEPROC ) platformGetProcAddress( "glGetProgramResourceName" ) ) != 0x0;
		r &= ( glGetProgramResourceLocation = ( PFNGLGETPROGRAMRESOURCELOCATIONPROC ) platformGetProcAddress( "glGetProgramResourceLocation" ) ) != 0x0;
		r &= ( glUseProgramStages = ( PFNGLUSEPROGRAMSTAGESPROC ) platformGetProcAddress( "glUseProgramStages" ) ) != 0x0;
		r &= ( glActiveShaderProgram = ( PFNGLACTIVESHADERPROGRAMPROC ) platformGetProcAddress( "glActiveShaderProgram" ) ) != 0x0;
		r &= ( glCreateShaderProgramv = ( PFNGLCREATESHADERPROGRAMVPROC ) platformGetProcAddress( "glCreateShaderProgramv" ) ) != 0x0;
		r &= ( glBindProgramPipeline = ( PFNGLBINDPROGRAMPIPELINEPROC ) platformGetProcAddress( "glBindProgramPipeline" ) ) != 0x0;
		r &= ( glDeleteProgramPipelines = ( PFNGLDELETEPROGRAMPIPELINESPROC ) platformGetProcAddress( "glDeleteProgramPipelines" ) ) != 0x0;
		r &= ( glGenProgramPipelines = ( PFNGLGENPROGRAMPIPELINESPROC ) platformGetProcAddress( "glGenProgramPipelines" ) ) != 0x0;
		r &= ( glIsProgramPipeline = ( PFNGLISPROGRAMPIPELINEPROC ) platformGetProcAddress( "glIsProgramPipeline" ) ) != 0x0;
		r &= ( glGetProgramPipelineiv = ( PFNGLGETPROGRAMPIPELINEIVPROC ) platformGetProcAddress( "glGetProgramPipelineiv" ) ) != 0x0;
		r &= ( glProgramUniform1i = ( PFNGLPROGRAMUNIFORM1IPROC ) platformGetProcAddress( "glProgramUniform1i" ) ) != 0x0;
		r &= ( glProgramUniform2i = ( PFNGLPROGRAMUNIFORM2IPROC ) platformGetProcAddress( "glProgramUniform2i" ) ) != 0x0;
		r &= ( glProgramUniform3i = ( PFNGLPROGRAMUNIFORM3IPROC ) platformGetProcAddress( "glProgramUniform3i" ) ) != 0x0;
		r &= ( glProgramUniform4i = ( PFNGLPROGRAMUNIFORM4IPROC ) platformGetProcAddress( "glProgramUniform4i" ) ) != 0x0;
		r &= ( glProgramUniform1ui = ( PFNGLPROGRAMUNIFORM1UIPROC ) platformGetProcAddress( "glProgramUniform1ui" ) ) != 0x0;
		r &= ( glProgramUniform2ui = ( PFNGLPROGRAMUNIFORM2UIPROC ) platformGetProcAddress( "glProgramUniform2ui" ) ) != 0x0;
		r &= ( glProgramUniform3ui = ( PFNGLPROGRAMUNIFORM3UIPROC ) platformGetProcAddress( "glProgramUniform3ui" ) ) != 0x0;
		r &= ( glProgramUniform4ui = ( PFNGLPROGRAMUNIFORM4UIPROC ) platformGetProcAddress( "glProgramUniform4ui" ) ) != 0x0;
		r &= ( glProgramUniform1f = ( PFNGLPROGRAMUNIFORM1FPROC ) platformGetProcAddress( "glProgramUniform1f" ) ) != 0x0;
		r &= ( glProgramUniform2f = ( PFNGLPROGRAMUNIFORM2FPROC ) platformGetProcAddress( "glProgramUniform2f" ) ) != 0x0;
		r &= ( glProgramUniform3f = ( PFNGLPROGRAMUNIFORM3FPROC ) platformGetProcAddress( "glProgramUniform3f" ) ) != 0x0;
		r &= ( glProgramUniform4f = ( PFNGLPROGRAMUNIFORM4FPROC ) platformGetProcAddress( "glProgramUniform4f" ) ) != 0x0;
		r &= ( glProgramUniform1iv = ( PFNGLPROGRAMUNIFORM1IVPROC ) platformGetProcAddress( "glProgramUniform1iv" ) ) != 0x0;
		r &= ( glProgramUniform2iv = ( PFNGLPROGRAMUNIFORM2IVPROC ) platformGetProcAddress( "glProgramUniform2iv" ) ) != 0x0;
		r &= ( glProgramUniform3iv = ( PFNGLPROGRAMUNIFORM3IVPROC ) platformGetProcAddress( "glProgramUniform3iv" ) ) != 0x0;
		r &= ( glProgramUniform4iv = ( PFNGLPROGRAMUNIFORM4IVPROC ) platformGetProcAddress( "glProgramUniform4iv" ) ) != 0x0;
		r &= ( glProgramUniform1uiv = ( PFNGLPROGRAMUNIFORM1UIVPROC ) platformGetProcAddress( "glProgramUniform1uiv" ) ) != 0x0;
		r &= ( glProgramUniform2uiv = ( PFNGLPROGRAMUNIFORM2UIVPROC ) platformGetProcAddress( "glProgramUniform2uiv" ) ) != 0x0;
		r &= ( glProgramUniform3uiv = ( PFNGLPROGRAMUNIFORM3UIVPROC ) platformGetProcAddress( "glProgramUniform3uiv" ) ) != 0x0;
		r &= ( glProgramUniform4uiv = ( PFNGLPROGRAMUNIFORM4UIVPROC ) platformGetProcAddress( "glProgramUniform4uiv" ) ) != 0x0;
		r &= ( glProgramUniform1fv = ( PFNGLPROGRAMUNIFORM1FVPROC ) platformGetProcAddress( "glProgramUniform1fv" ) ) != 0x0;
		r &= ( glProgramUniform2fv = ( PFNGLPROGRAMUNIFORM2FVPROC ) platformGetProcAddress( "glProgramUniform2fv" ) ) != 0x0;
		r &= ( glProgramUniform3fv = ( PFNGLPROGRAMUNIFORM3FVPROC ) platformGetProcAddress( "glProgramUniform3fv" ) ) != 0x0;
		r &= ( glProgramUniform4fv = ( PFNGLPROGRAMUNIFORM4FVPROC ) platformGetProcAddress( "glProgramUniform4fv" ) ) != 0x0;
		r &= ( glProgramUniformMatrix2fv = ( PFNGLPROGRAMUNIFORMMATRIX2FVPROC ) platformGetProcAddress( "glProgramUniformMatrix2fv" ) ) != 0x0;
		r &= ( glProgramUniformMatrix3fv = ( PFNGLPROGRAMUNIFORMMATRIX3FVPROC ) platformGetProcAddress( "glProgramUniformMatrix3fv" ) ) != 0x0;
		r &= ( glProgramUniformMatrix4fv = ( PFNGLPROGRAMUNIFORMMATRIX4FVPROC ) platformGetProcAddress( "glProgramUniformMatrix4fv" ) ) != 0x0;
		r &= ( glProgramUniformMatrix2x3fv = ( PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC ) platformGetProcAddress( "glProgramUniformMatrix2x3fv" ) ) != 0x0;
		r &= ( glProgramUniformMatrix3x2fv = ( PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC ) platformGetProcAddress( "glProgramUniformMatrix3x2fv" ) ) != 0x0;
		r &= ( glProgramUniformMatrix2x4fv = ( PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC ) platformGetProcAddress( "glProgramUniformMatrix2x4fv" ) ) != 0x0;
		r &= ( glProgramUniformMatrix4x2fv = ( PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC ) platformGetProcAddress( "glProgramUniformMatrix4x2fv" ) ) != 0x0;
		r &= ( glProgramUniformMatrix3x4fv = ( PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC ) platformGetProcAddress( "glProgramUniformMatrix3x4fv" ) ) != 0x0;
		r &= ( glProgramUniformMatrix4x3fv = ( PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC ) platformGetProcAddress( "glProgramUniformMatrix4x3fv" ) ) != 0x0;
		r &= ( glValidateProgramPipeline = ( PFNGLVALIDATEPROGRAMPIPELINEPROC ) platformGetProcAddress( "glValidateProgramPipeline" ) ) != 0x0;
		r &= ( glGetProgramPipelineInfoLog = ( PFNGLGETPROGRAMPIPELINEINFOLOGPROC ) platformGetProcAddress( "glGetProgramPipelineInfoLog" ) ) != 0x0;
		r &= ( glBindImageTexture = ( PFNGLBINDIMAGETEXTUREPROC ) platformGetProcAddress( "glBindImageTexture" ) ) != 0x0;
		r &= ( glGetBooleani_v = ( PFNGLGETBOOLEANI_VPROC ) platformGetProcAddress( "glGetBooleani_v" ) ) != 0x0;
		r &= ( glMemoryBarrierByRegion = ( PFNGLMEMORYBARRIERBYREGIONPROC ) platformGetProcAddress( "glMemoryBarrierByRegion" ) ) != 0x0;
		r &= ( glTexStorage2DMultisample = ( PFNGLTEXSTORAGE2DMULTISAMPLEPROC ) platformGetProcAddress( "glTexStorage2DMultisample" ) ) != 0x0;
		r &= ( glGetMultisamplefv = ( PFNGLGETMULTISAMPLEFVPROC ) platformGetProcAddress( "glGetMultisamplefv" ) ) != 0x0;
		r &= ( glSampleMaski = ( PFNGLSAMPLEMASKIPROC ) platformGetProcAddress( "glSampleMaski" ) ) != 0x0;
		r &= ( glGetTexLevelParameteriv = ( PFNGLGETTEXLEVELPARAMETERIVPROC ) platformGetProcAddress( "glGetTexLevelParameteriv" ) ) != 0x0;
		r &= ( glGetTexLevelParameterfv = ( PFNGLGETTEXLEVELPARAMETERFVPROC ) platformGetProcAddress( "glGetTexLevelParameterfv" ) ) != 0x0;
		r &= ( glBindVertexBuffer = ( PFNGLBINDVERTEXBUFFERPROC ) platformGetProcAddress( "glBindVertexBuffer" ) ) != 0x0;
		r &= ( glVertexAttribFormat = ( PFNGLVERTEXATTRIBFORMATPROC ) platformGetProcAddress( "glVertexAttribFormat" ) ) != 0x0;
		r &= ( glVertexAttribIFormat = ( PFNGLVERTEXATTRIBIFORMATPROC ) platformGetProcAddress( "glVertexAttribIFormat" ) ) != 0x0;
		r &= ( glVertexAttribBinding = ( PFNGLVERTEXATTRIBBINDINGPROC ) platformGetProcAddress( "glVertexAttribBinding" ) ) != 0x0;
		r &= ( glVertexBindingDivisor = ( PFNGLVERTEXBINDINGDIVISORPROC ) platformGetProcAddress( "glVertexBindingDivisor" ) ) != 0x0;
	}

	// Core ES 3.2
	if ( glESExt::majorVersion * 10 + glESExt::minorVersion >= 32 )
	{
		r &= ( glBlendBarrier = ( PFNGLBLENDBARRIERPROC ) platformGetProcAddress( "glBlendBarrier" ) ) != 0x0;
		r &= ( glCopyImageSubData = ( PFNGLCOPYIMAGESUBDATAPROC ) platformGetProcAddress( "glCopyImageSubData" ) ) != 0x0;
		r &= ( glDebugMessageControl = ( PFNGLDEBUGMESSAGECONTROLPROC ) platformGetProcAddress( "glDebugMessageControl" ) ) != 0x0;
		r &= ( glDebugMessageInsert = ( PFNGLDEBUGMESSAGEINSERTPROC ) platformGetProcAddress( "glDebugMessageInsert" ) ) != 0x0;
		r &= ( glDebugMessageCallback = ( PFNGLDEBUGMESSAGECALLBACKPROC ) platformGetProcAddress( "glDebugMessageCallback" ) ) != 0x0;
		r &= ( glGetDebugMessageLog = ( PFNGLGETDEBUGMESSAGELOGPROC ) platformGetProcAddress( "glGetDebugMessageLog" ) ) != 0x0;
		r &= ( glPushDebugGroup = ( PFNGLPUSHDEBUGGROUPPROC ) platformGetProcAddress( "glPushDebugGroup" ) ) != 0x0;
		r &= ( glPopDebugGroup = ( PFNGLPOPDEBUGGROUPPROC ) platformGetProcAddress( "glPopDebugGroup" ) ) != 0x0;
		r &= ( glObjectLabel = ( PFNGLOBJECTLABELPROC ) platformGetProcAddress( "glObjectLabel" ) ) != 0x0;
		r &= ( glGetObjectLabel = ( PFNGLGETOBJECTLABELPROC ) platformGetProcAddress( "glGetObjectLabel" ) ) != 0x0;
		r &= ( glObjectPtrLabel = ( PFNGLOBJECTPTRLABELPROC ) platformGetProcAddress( "glObjectPtrLabel" ) ) != 0x0;
		r &= ( glGetObjectPtrLabel = ( PFNGLGETOBJECTPTRLABELPROC ) platformGetProcAddress( "glGetObjectPtrLabel" ) ) != 0x0;
		r &= ( glGetPointerv = ( PFNGLGETPOINTERVPROC ) platformGetProcAddress( "glGetPointerv" ) ) != 0x0;
		r &= ( glEnablei = ( PFNGLENABLEIPROC ) platformGetProcAddress( "glEnablei" ) ) != 0x0;
		r &= ( glDisablei = ( PFNGLDISABLEIPROC ) platformGetProcAddress( "glDisablei" ) ) != 0x0;
		r &= ( glBlendEquationi = ( PFNGLBLENDEQUATIONIPROC ) platformGetProcAddress( "glBlendEquationi" ) ) != 0x0;
		r &= ( glBlendEquationSeparatei = ( PFNGLBLENDEQUATIONSEPARATEIPROC ) platformGetProcAddress( "glBlendEquationSeparatei" ) ) != 0x0;
		r &= ( glBlendFunci = ( PFNGLBLENDFUNCIPROC ) platformGetProcAddress( "glBlendFunci" ) ) != 0x0;
		r &= ( glBlendFuncSeparatei = ( PFNGLBLENDFUNCSEPARATEIPROC ) platformGetProcAddress( "glBlendFuncSeparatei" ) ) != 0x0;
		r &= ( glColorMaski = ( PFNGLCOLORMASKIPROC ) platformGetProcAddress( "glColorMaski" ) ) != 0x0;
		r &= ( glIsEnabledi = ( PFNGLISENABLEDIPROC ) platformGetProcAddress( "glIsEnabledi" ) ) != 0x0;
		r &= ( glDrawElementsBaseVertex = ( PFNGLDRAWELEMENTSBASEVERTEXPROC ) platformGetProcAddress( "glDrawElementsBaseVertex" ) ) != 0x0;
		r &= ( glDrawRangeElementsBaseVertex = ( PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC ) platformGetProcAddress( "glDrawRangeElementsBaseVertex" ) ) != 0x0;
		r &= ( glDrawElementsInstancedBaseVertex = ( PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC ) platformGetProcAddress( "glDrawElementsInstancedBaseVertex" ) ) != 0x0;
		r &= ( glFramebufferTexture = ( PFNGLFRAMEBUFFERTEXTUREPROC ) platformGetProcAddress( "glFramebufferTexture" ) ) != 0x0;
		r &= ( glPrimitiveBoundingBox = ( PFNGLPRIMITIVEBOUNDINGBOXPROC ) platformGetProcAddress( "glPrimitiveBoundingBox" ) ) != 0x0;
		r &= ( glGetGraphicsResetStatus = ( PFNGLGETGRAPHICSRESETSTATUSPROC ) platformGetProcAddress( "glGetGraphicsResetStatus" ) ) != 0x0;
		r &= ( glReadnPixels = ( PFNGLREADNPIXELSPROC ) platformGetProcAddress( "glReadnPixels" ) ) != 0x0;
		r &= ( glGetnUniformfv = ( PFNGLGETNUNIFORMFVPROC ) platformGetProcAddress( "glGetnUniformfv" ) ) != 0x0;
		r &= ( glGetnUniformiv = ( PFNGLGETNUNIFORMIVPROC ) platformGetProcAddress( "glGetnUniformiv" ) ) != 0x0;
		r &= ( glGetnUniformuiv = ( PFNGLGETNUNIFORMUIVPROC ) platformGetProcAddress( "glGetnUniformuiv" ) ) != 0x0;
		r &= ( glMinSampleShading = ( PFNGLMINSAMPLESHADINGPROC ) platformGetProcAddress( "glMinSampleShading" ) ) != 0x0;
		r &= ( glTexParameterIiv = ( PFNGLTEXPARAMETERIIVPROC ) platformGetProcAddress( "glTexParameterIiv" ) ) != 0x0;
		r &= ( glTexParameterIuiv = ( PFNGLTEXPARAMETERIUIVPROC ) platformGetProcAddress( "glTexParameterIuiv" ) ) != 0x0;
		r &= ( glGetTexParameterIiv = ( PFNGLGETTEXPARAMETERIIVPROC ) platformGetProcAddress( "glGetTexParameterIiv" ) ) != 0x0;
		r &= ( glGetTexParameterIuiv = ( PFNGLGETTEXPARAMETERIUIVPROC ) platformGetProcAddress( "glGetTexParameterIuiv" ) ) != 0x0;
		r &= ( glSamplerParameterIiv = ( PFNGLSAMPLERPARAMETERIIVPROC ) platformGetProcAddress( "glSamplerParameterIiv" ) ) != 0x0;
		r &= ( glSamplerParameterIuiv = ( PFNGLSAMPLERPARAMETERIUIVPROC ) platformGetProcAddress( "glSamplerParameterIuiv" ) ) != 0x0;
		r &= ( glGetSamplerParameterIiv = ( PFNGLGETSAMPLERPARAMETERIIVPROC ) platformGetProcAddress( "glGetSamplerParameterIiv" ) ) != 0x0;
		r &= ( glGetSamplerParameterIuiv = ( PFNGLGETSAMPLERPARAMETERIUIVPROC ) platformGetProcAddress( "glGetSamplerParameterIuiv" ) ) != 0x0;
		r &= ( glTexBuffer = ( PFNGLTEXBUFFERPROC ) platformGetProcAddress( "glTexBuffer" ) ) != 0x0;
		r &= ( glTexBufferRange = ( PFNGLTEXBUFFERRANGEPROC ) platformGetProcAddress( "glTexBufferRange" ) ) != 0x0;
		r &= ( glTexStorage3DMultisample = ( PFNGLTEXSTORAGE3DMULTISAMPLEPROC ) platformGetProcAddress( "glTexStorage3DMultisample" ) ) != 0x0;
	}

	// Extensions: best-effort. A flag só fica 1 se a extensão está anunciada (ou incluída no
	// core da versão) E as funções carregaram - há drivers que anunciam sem exportar o símbolo
	// (ex.: o Mesa anuncia OES_texture_3D e OES_EGL_image_external sem as funções).
	glESExt::EXT_texture_filter_anisotropic = checkExtensionSupported( "GL_EXT_texture_filter_anisotropic" );
	glESExt::EXT_texture_compression_s3tc = checkExtensionSupported( "GL_EXT_texture_compression_s3tc" );
	glESExt::EXT_texture_compression_dxt1 = checkExtensionSupported( "GL_EXT_texture_compression_dxt1" );
	glESExt::EXT_texture_compression_bptc = checkExtensionSupported( "GL_EXT_texture_compression_bptc" );
	glESExt::KHR_texture_compression_astc = checkExtensionSupported( "GL_KHR_texture_compression_astc_ldr" );

	glESExt::KHR_debug = checkExtensionSupported( "GL_KHR_debug" );
	if ( glESExt::KHR_debug )
	{
		glDebugMessageCallbackKHR = ( PFNGLDEBUGMESSAGECALLBACKKHRPROC ) platformGetProcAddressAny( "glDebugMessageCallbackKHR", "glDebugMessageCallback" );
		glDebugMessageControlKHR = ( PFNGLDEBUGMESSAGECONTROLKHRPROC ) platformGetProcAddressAny( "glDebugMessageControlKHR", "glDebugMessageControl" );
		glDebugMessageInsertKHR = ( PFNGLDEBUGMESSAGEINSERTKHRPROC ) platformGetProcAddressAny( "glDebugMessageInsertKHR", "glDebugMessageInsert" );
		glGetDebugMessageLogKHR = ( PFNGLGETDEBUGMESSAGELOGKHRPROC ) platformGetProcAddressAny( "glGetDebugMessageLogKHR", "glGetDebugMessageLog" );
		glESExt::KHR_debug = glDebugMessageCallbackKHR && glDebugMessageControlKHR && glDebugMessageInsertKHR && glGetDebugMessageLogKHR;
	}

	glESExt::EXT_disjoint_timer_query = checkExtensionSupported( "GL_EXT_disjoint_timer_query" );
	if ( glESExt::EXT_disjoint_timer_query )
	{
		glQueryCounterEXT = ( PFNGLQUERYCOUNTEREXTPROC ) platformGetProcAddress( "glQueryCounterEXT" );
		glGetQueryObjectivEXT = ( PFNGLGETQUERYOBJECTIVEXTPROC ) platformGetProcAddress( "glGetQueryObjectivEXT" );
		glGetQueryObjectui64vEXT = ( PFNGLGETQUERYOBJECTUI64VEXTPROC ) platformGetProcAddress( "glGetQueryObjectui64vEXT" );
		glESExt::EXT_disjoint_timer_query = glQueryCounterEXT && glGetQueryObjectivEXT && glGetQueryObjectui64vEXT;
	}

	glESExt::EXT_tessellation_shader = checkExtensionSupported( "GL_EXT_tessellation_shader" ) || checkExtensionSupported( "GL_OES_tessellation_shader" );
	if ( glESExt::EXT_tessellation_shader )
	{
		glPatchParameteri = ( PFNGLPATCHPARAMETERIPROC ) platformGetProcAddress( "glPatchParameteri" );
		glESExt::EXT_tessellation_shader = glPatchParameteri != 0x0;
	}
	if ( !glPatchParameteri && glESExt::majorVersion * 10 + glESExt::minorVersion >= 32 )
	{
		glPatchParameteri = ( PFNGLPATCHPARAMETERIPROC ) platformGetProcAddress( "glPatchParameteri" );
	}

	glESExt::EXT_color_buffer_float = checkExtensionSupported( "GL_EXT_color_buffer_float" ) || checkExtensionSupported( "GL_EXT_color_buffer_half_float" );
	glESExt::OES_compressed_ETC1_RGB8_texture = checkExtensionSupported( "GL_OES_compressed_ETC1_RGB8_texture" );
	glESExt::EXT_texture_border_clamp = checkExtensionSupported( "GL_EXT_texture_border_clamp" ) ||
	                                    checkExtensionSupported( "GL_OES_texture_border_clamp" ) ||
	                                    glESExt::majorVersion * 10 + glESExt::minorVersion >= 32;
	if ( glESExt::EXT_texture_border_clamp )
	{
		glTexParameterIivEXT = ( PFNGLTEXPARAMETERIIVEXTPROC ) platformGetProcAddressAny( "glTexParameterIivEXT", "glTexParameterIivOES", "glTexParameterIiv" );
		glTexParameterIuivEXT = ( PFNGLTEXPARAMETERIUIVEXTPROC ) platformGetProcAddressAny( "glTexParameterIuivEXT", "glTexParameterIuivOES", "glTexParameterIuiv" );
		glGetTexParameterIivEXT = ( PFNGLGETTEXPARAMETERIIVEXTPROC ) platformGetProcAddressAny( "glGetTexParameterIivEXT", "glGetTexParameterIivOES", "glGetTexParameterIiv" );
		glGetTexParameterIuivEXT = ( PFNGLGETTEXPARAMETERIUIVEXTPROC ) platformGetProcAddressAny( "glGetTexParameterIuivEXT", "glGetTexParameterIuivOES", "glGetTexParameterIuiv" );
		glSamplerParameterIivEXT = ( PFNGLSAMPLERPARAMETERIIVEXTPROC ) platformGetProcAddressAny( "glSamplerParameterIivEXT", "glSamplerParameterIivOES", "glSamplerParameterIiv" );
		glSamplerParameterIuivEXT = ( PFNGLSAMPLERPARAMETERIUIVEXTPROC ) platformGetProcAddressAny( "glSamplerParameterIuivEXT", "glSamplerParameterIuivOES", "glSamplerParameterIuiv" );
		glGetSamplerParameterIivEXT = ( PFNGLGETSAMPLERPARAMETERIIVEXTPROC ) platformGetProcAddressAny( "glGetSamplerParameterIivEXT", "glGetSamplerParameterIivOES", "glGetSamplerParameterIiv" );
		glGetSamplerParameterIuivEXT = ( PFNGLGETSAMPLERPARAMETERIUIVEXTPROC ) platformGetProcAddressAny( "glGetSamplerParameterIuivEXT", "glGetSamplerParameterIuivOES", "glGetSamplerParameterIuiv" );
		glESExt::EXT_texture_border_clamp = glTexParameterIivEXT && glTexParameterIuivEXT && glGetTexParameterIivEXT && glGetTexParameterIuivEXT &&
		                                    glSamplerParameterIivEXT && glSamplerParameterIuivEXT && glGetSamplerParameterIivEXT && glGetSamplerParameterIuivEXT;
	}

	glESExt::EXT_geometry_shader = checkExtensionSupported( "GL_EXT_geometry_shader" ) || checkExtensionSupported( "GL_OES_geometry_shader" );

	glESExt::OES_texture_3D = checkExtensionSupported( "GL_OES_texture_3D" );
	if ( glESExt::OES_texture_3D )
	{
		glFramebufferTexture3DOES = ( PFNGLFRAMEBUFFERTEXTURE3DOESPROC ) platformGetProcAddress( "glFramebufferTexture3DOES" );
		glESExt::OES_texture_3D = glFramebufferTexture3DOES != 0x0;
	}

	glESExt::OES_EGL_image_external = checkExtensionSupported( "GL_OES_EGL_image_external" );
	if ( glESExt::OES_EGL_image_external )
	{
		glEGLImageTargetTexture2DOES = ( PFNGLEGLIMAGETARGETTEXTURE2DOESPROC ) platformGetProcAddress( "glEGLImageTargetTexture2DOES" );
		glESExt::OES_EGL_image_external = glEGLImageTargetTexture2DOES != 0x0;
	}

	return r;
}
