/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture_GLES3.cpp
Content     :   Oculus performance capture library. OpenGL ES 3 interfaces.
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include <OVR_Capture_GLES3.h>

#if defined(OVR_CAPTURE_HAS_GLES3)

#include <OVR_Capture_Packets.h>
#include "OVR_Capture_AsyncStream.h"

#include <string.h> // memset
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <EGL/egl.h>

namespace OVR
{
namespace Capture
{

    struct PendingFrameBuffer
    {
        UInt64 timestamp; // TODO: currently this isn't used!
        GLuint renderbuffer;
        GLuint fbo;
        GLuint pbo;
        bool   imageReady;
    };

    static bool                    g_requireCleanup         = false;

    static const unsigned int      g_maxPendingFramebuffers = 2;
    static unsigned int            g_nextPendingFramebuffer = 0;
    static PendingFrameBuffer      g_pendingFramebuffers[g_maxPendingFramebuffers];

    static const unsigned int      g_width         = 128;
    static const unsigned int      g_height        = 128;
    static const unsigned int      g_imageSize     = g_width * g_height * 2;
    static const FrameBufferFormat g_imageFormat   = FrameBuffer_RGB_565;
    static const GLenum            g_imageFormatGL = GL_RGB565;

    static GLuint                  g_vertexShader   = 0;
    static GLuint                  g_fragmentShader = 0;
    static GLuint                  g_program        = 0;

    static GLuint                  g_vertexBuffer      = 0;
    static GLuint                  g_vertexArrayObject = 0;

    enum ShaderAttributes
    {
        POSITION_ATTRIBUTE = 0,
        TEXCOORD_ATTRIBUTE,
    };

    static const char *g_vertexShaderSource = 
        "attribute vec4 Position;\n"
        "attribute vec2 TexCoord;\n"
        "varying  highp vec2 oTexCoord;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = Position;\n"
        "   oTexCoord = vec2(TexCoord.x, 1.0 - TexCoord.y);\n"    // need to flip Y
        "}\n";

    static const char *g_fragmentShaderSource =
        "uniform sampler2D Texture0;\n"
        "varying highp vec2 oTexCoord;\n"
        "void main()\n"
        "{\n"
        "   gl_FragColor = texture2D(Texture0, oTexCoord);\n"
        "}\n";
    
    static const float g_vertices[] =
    {
       -1.0f,-1.0f, 0.0f, 1.0f,  0.0f, 0.0f,
        3.0f,-1.0f, 0.0f, 1.0f,  2.0f, 0.0f,
       -1.0f, 3.0f, 0.0f, 1.0f,  0.0f, 2.0f,
    };


    // Clean up OpenGL state... MUST be called from the GL thread! Which is why Capture::Shutdown() can't safely call this.
    static void CleanupGLES3(void)
    {
        if(g_program)
        {
            glDeleteProgram(g_program);
            g_program = 0;
        }
        if(g_vertexShader)
        {
            glDeleteShader(g_vertexShader);
            g_vertexShader = 0;
        }
        if(g_fragmentShader)
        {
            glDeleteShader(g_fragmentShader);
            g_fragmentShader = 0;
        }
        if(g_vertexArrayObject)
        {
            glDeleteVertexArrays(1, &g_vertexArrayObject);
            g_vertexArrayObject = 0;
        }
        if(g_vertexBuffer)
        {
            glDeleteBuffers(1, &g_vertexBuffer);
            g_vertexBuffer = 0;
        }
        for(unsigned int i=0; i<g_maxPendingFramebuffers; i++)
        {
            PendingFrameBuffer &fb = g_pendingFramebuffers[i];
            if(fb.renderbuffer) glDeleteRenderbuffers(1, &fb.renderbuffer);
            if(fb.fbo)          glDeleteFramebuffers( 1, &fb.fbo);
            if(fb.pbo)          glDeleteBuffers(      1, &fb.pbo);
            memset(&fb, 0, sizeof(fb));
        }
        g_requireCleanup = false;
    }

    // Captures the frame buffer from a Texture Object from an OpenGL ES 3.0 Context.
    // Must be called from a thread with a valid GLES3 context!
    void FrameBufferGLES3(unsigned int textureID)
    {
        if(!CheckConnectionFlag(Enable_FrameBuffer_Capture))
        {
            if(g_requireCleanup)
                CleanupGLES3();
            return;
        }

        OVR_CAPTURE_CPU_ZONE(FrameBufferGLES3);

        // once a connection is stablished we should flag ourselves for cleanup...
        g_requireCleanup = true;

        // Basic Concept:
        //   0) Capture Time Stamp
        //   1) StretchBlit into lower resolution 565 texture
        //   2) Issue async ReadPixels into pixel buffer object
        //   3) Wait N Frames
        //   4) Map PBO memory and call FrameBuffer(g_imageFormat,g_width,g_height,imageData)

        // acquire current time before spending cycles mapping and copying the PBO...
        const UInt64 currentTime = GetNanoseconds();

        // Acquire a PendingFrameBuffer container...
        PendingFrameBuffer &fb = g_pendingFramebuffers[g_nextPendingFramebuffer];

        // If the pending framebuffer has valid data in it... lets send it over the network...
        if(fb.imageReady)
        {
            OVR_CAPTURE_CPU_ZONE(MapAndCopy);
            // 4) Map PBO memory and call FrameBuffer(g_imageFormat,g_width,g_height,imageData)
            glBindBuffer(GL_PIXEL_PACK_BUFFER, fb.pbo);
            const void *mappedImage = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, g_imageSize, GL_MAP_READ_BIT);
            FrameBuffer(fb.timestamp, g_imageFormat, g_width, g_height, mappedImage);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            fb.imageReady = false;
        }

        // 0) Capture Time Stamp
        fb.timestamp = currentTime;


        // Create GL objects if necessary...
        if(!fb.renderbuffer)
        {
            glGenRenderbuffers(1, &fb.renderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, fb.renderbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, g_imageFormatGL, g_width, g_height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
        if(!fb.fbo)
        {
            glGenFramebuffers(1, &fb.fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fb.renderbuffer);
            const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if(status != GL_FRAMEBUFFER_COMPLETE)
            {
                // ERROR!
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        if(!fb.pbo)
        {
            glGenBuffers(1, &fb.pbo);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, fb.pbo);
            glBufferData(GL_PIXEL_PACK_BUFFER, g_imageSize, NULL, GL_DYNAMIC_READ);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }

        // Create shader if necessary...
        if(!g_vertexShader)
        {
            g_vertexShader = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(g_vertexShader, 1, &g_vertexShaderSource, 0);
            glCompileShader(g_vertexShader);
            GLint success = 0;
            glGetShaderiv(g_vertexShader, GL_COMPILE_STATUS, &success);
            if(success == GL_FALSE)
            {
                Logf(Log_Error, "OVR_Capture_GLES3: Failed to compile Vertex Shader!");
                glDeleteShader(g_vertexShader);
                g_vertexShader = 0;
            }
        }
        if(!g_fragmentShader)
        {
            g_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(g_fragmentShader, 1, &g_fragmentShaderSource, 0);
            glCompileShader(g_fragmentShader);
            GLint success = 0;
            glGetShaderiv(g_fragmentShader, GL_COMPILE_STATUS, &success);
            if(success == GL_FALSE)
            {
                Logf(Log_Error, "OVR_Capture_GLES3: Failed to compile Fragment Shader!");
                glDeleteShader(g_fragmentShader);
                g_fragmentShader = 0;
            }
        }
        if(!g_program && g_vertexShader && g_fragmentShader)
        {
            g_program = glCreateProgram();
            glAttachShader(g_program, g_vertexShader);
            glAttachShader(g_program, g_fragmentShader);
            glBindAttribLocation(g_program, POSITION_ATTRIBUTE, "Position" );
            glBindAttribLocation(g_program, TEXCOORD_ATTRIBUTE, "TexCoord" );
            glLinkProgram(g_program);
            GLint success = 0;
            glGetProgramiv(g_program, GL_LINK_STATUS, &success);
            if(success == GL_FALSE)
            {
                Logf(Log_Error, "OVR_Capture_GLES3: Failed to link Program!");
                glDeleteProgram(g_program);
                g_program = 0;
            }
            else
            {
                glUseProgram(g_program);
                glUniform1i(glGetUniformLocation(g_program, "Texture0"), 0);
                glUseProgram(0);
            }
        }

        // Create Vertex Array...
        if(!g_vertexArrayObject)
        {
            glGenVertexArrays(1, &g_vertexArrayObject);
            glBindVertexArray(g_vertexArrayObject);

            glGenBuffers(1, &g_vertexBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, g_vertexBuffer);
            glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertices), g_vertices, GL_STATIC_DRAW);

            glEnableVertexAttribArray(POSITION_ATTRIBUTE);
            glEnableVertexAttribArray(TEXCOORD_ATTRIBUTE);
            glVertexAttribPointer(POSITION_ATTRIBUTE, 4, GL_FLOAT, GL_FALSE, sizeof(float)*6, ((const char*)NULL)+sizeof(float)*0);
            glVertexAttribPointer(TEXCOORD_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, sizeof(float)*6, ((const char*)NULL)+sizeof(float)*4);
        }

        // Verify all the objects we need are allocated...
        if(!fb.renderbuffer || !fb.fbo || !fb.pbo || !g_program || !g_vertexArrayObject)
            return;

        // 1) StretchBlit into lower resolution 565 texture
        glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
        const GLenum attachments[1] = { GL_COLOR_ATTACHMENT0 };
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, attachments);

        glViewport(0, 0, g_width, g_height);

        // These are the GLenum states that we want disabled for our blit...
        static const GLenum disabledState[] =
        {
            GL_DEPTH_TEST,
            GL_SCISSOR_TEST,
            GL_STENCIL_TEST,
            GL_RASTERIZER_DISCARD,
            GL_DITHER,
            GL_CULL_FACE, // turning culling off entirely is one less state than setting winding order and front face.
            GL_BLEND,
        };
        static const GLuint disabledStateCount = sizeof(disabledState) / sizeof(disabledState[0]);

        GLboolean disabledStatePreviousValue[disabledStateCount] = {0};
        GLboolean previousColorMask[4]                           = {0};

        if(true)
        {
            OVR_CAPTURE_CPU_ZONE(LoadState);
            // Load previous state and disable only if necessary...
            for(GLuint i=0; i<disabledStateCount; i++)
            {
                disabledStatePreviousValue[i] = glIsEnabled(disabledState[i]);
            }
            // Save/override color write mask state...
            glGetBooleanv(GL_COLOR_WRITEMASK, previousColorMask);
        }
        if(true)
        {
            OVR_CAPTURE_CPU_ZONE(OverrideState);
            for(GLuint i=0; i<disabledStateCount; i++)
            {
                if(disabledStatePreviousValue[i])
                    glDisable(disabledState[i]);
            }
            if(previousColorMask[0]!=GL_TRUE || previousColorMask[1]!=GL_TRUE || previousColorMask[2]!=GL_TRUE)
            {
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
            }
        }
        
        // Useful for detecting GL state leaks that kill drawing...
        // Because only a few state bits affect glClear (pixel ownership, dithering, color mask, and scissor), enabling this
        // helps narrow down if a the FB is not being updated because of further state leaks that affect rendering geometry,
        // or if its another issue.
        //glClearColor(0,0,1,1);
        //glClear(GL_COLOR_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glUseProgram(g_program);

        glBindVertexArray(g_vertexArrayObject);

        if(true)
        {
            OVR_CAPTURE_CPU_ZONE(DrawArrays);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        glUseProgram(0);

        // 2) Issue async ReadPixels into pixel buffer object
        glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, fb.pbo);
        glReadPixels(0, 0, g_width, g_height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        fb.imageReady = true;

        if(true)
        {
            OVR_CAPTURE_CPU_ZONE(RestoreState);
            // Restore previous state...
            for(GLuint i=0; i<disabledStateCount; i++)
            {
                if(disabledStatePreviousValue[i])
                    glEnable(disabledState[i]);
            }
            if(previousColorMask[0]!=GL_TRUE || previousColorMask[1]!=GL_TRUE || previousColorMask[2]!=GL_TRUE)
            {
                glColorMask(previousColorMask[0], previousColorMask[1], previousColorMask[2], previousColorMask[3]);
            }
        }

        // Increment to the next PendingFrameBuffer...
        g_nextPendingFramebuffer = (g_nextPendingFramebuffer+1)&(g_maxPendingFramebuffers-1);
    }


    // We will have a circular buffer of GPU timer queries which will will take overship of inside Enter/Leave...
    // And every frame we will iterate through all the completed queries and dispatch the ones that are complete...
    // The circiular buffer size therefor should be at least big enough to hold all the outstanding queries for about 2 frames.
    // On Qualcomm chips we will need to check for disjoint events and send a different packet when that happens... as this indicates
    // that the timings are unreliable. This is probably also a worthwhile event to mark in the timeline as it probably will
    // coincide with a hitch or poor performance event.
    // EXT_disjoint_timer_query
    // https://www.khronos.org/registry/gles/extensions/EXT/EXT_disjoint_timer_query.txt

    #define GL_QUERY_COUNTER_BITS_EXT         0x8864
    #define GL_CURRENT_QUERY_EXT              0x8865
    #define GL_QUERY_RESULT_EXT               0x8866
    #define GL_QUERY_RESULT_AVAILABLE_EXT     0x8867
    #define GL_TIME_ELAPSED_EXT               0x88BF
    #define GL_TIMESTAMP_EXT                  0x8E28
    #define GL_GPU_DISJOINT_EXT               0x8FBB

    typedef void      (*PFNGLGENQUERIESEXTPROC)         (GLsizei n,       GLuint *ids);
    typedef void      (*PFNGLDELETEQUERIESEXTPROC)      (GLsizei n, const GLuint *ids);
    typedef GLboolean (*PFNGLISQUERYEXTPROC)            (GLuint id);
    typedef void      (*PFNGLBEGINQUERYEXTPROC)         (GLenum target, GLuint id);
    typedef void      (*PFNGLENDQUERYEXTPROC)           (GLenum target);
    typedef void      (*PFNGLQUERYCOUNTEREXTPROC)       (GLuint id, GLenum target);
    typedef void      (*PFNGLGETQUERYIVEXTPROC)         (GLenum target, GLenum pname, GLint    *params);
    typedef void      (*PFNGLGETQUERYOBJECTIVEXTPROC)   (GLuint id,     GLenum pname, GLint    *params);
    typedef void      (*PFNGLGETQUERYOBJECTUIVEXTPROC)  (GLuint id,     GLenum pname, GLuint   *params);
    typedef void      (*PFNGLGETQUERYOBJECTI64VEXTPROC) (GLuint id,     GLenum pname, GLint64  *params);
    typedef void      (*PFNGLGETQUERYOBJECTUI64VEXTPROC)(GLuint id,     GLenum pname, GLuint64 *params);
    typedef void      (*PFNGLGETINTEGER64VPROC)         (GLenum pname,                GLint64  *params);


    static PFNGLGENQUERIESEXTPROC          glGenQueriesEXT_          = NULL;
    static PFNGLDELETEQUERIESEXTPROC       glDeleteQueriesEXT_       = NULL;
    static PFNGLISQUERYEXTPROC             glIsQueryEXT_             = NULL;
    static PFNGLBEGINQUERYEXTPROC          glBeginQueryEXT_          = NULL;
    static PFNGLENDQUERYEXTPROC            glEndQueryEXT_            = NULL;
    static PFNGLQUERYCOUNTEREXTPROC        glQueryCounterEXT_        = NULL;
    static PFNGLGETQUERYIVEXTPROC          glGetQueryivEXT_          = NULL;
    static PFNGLGETQUERYOBJECTIVEXTPROC    glGetQueryObjectivEXT_    = NULL;
    static PFNGLGETQUERYOBJECTUIVEXTPROC   glGetQueryObjectuivEXT_   = NULL;
    static PFNGLGETQUERYOBJECTI64VEXTPROC  glGetQueryObjecti64vEXT_  = NULL;
    static PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64vEXT_ = NULL;
    static PFNGLGETINTEGER64VPROC          glGetInteger64v_          = NULL;

    static bool g_timerQueryInitialized        = false;
    static bool g_has_EXT_disjoint_timer_query = false;

    struct TimerQuery
    {
        GLuint queryID;
        UInt32 labelID; // 0 for Leave event...
    };

    static const UInt32 g_queryRingSize  = 128;
    static UInt32       g_queryRingCount = 0;
    static TimerQuery   g_queryRing[g_queryRingSize];
    static TimerQuery  *g_queryRingBegin             = g_queryRing;
    static TimerQuery  *g_queryRingEnd               = g_queryRingBegin + g_queryRingSize;
    static TimerQuery  *g_queryRingHead              = g_queryRingBegin; // head = next element we read from
    static TimerQuery  *g_queryRingTail              = g_queryRingBegin; // tail = next element we write to

    static TimerQuery *NextTimerQuery(TimerQuery *curr)
    {
        curr++;
        if(curr == g_queryRingEnd)
            curr = g_queryRingBegin;
        return curr;
    }

    static bool IsQueryRingEmpty(void)
    {
        return g_queryRingCount==0;
    }

    static bool IsQueryRingFull(void)
    {
        return g_queryRingCount==g_queryRingSize;
    }

    static TimerQuery *QueryRingAlloc(void)
    {
        TimerQuery *query = NULL;
        if(!IsQueryRingFull())
        {
            query = g_queryRingTail;
            g_queryRingTail = NextTimerQuery(g_queryRingTail);
            g_queryRingCount++;
        }
        return query;
    }

    static TimerQuery *QueryRingHead(void)
    {
        return IsQueryRingEmpty() ? NULL : g_queryRingHead;
    }

    static void QueryRingPop(void)
    {
        if(!IsQueryRingEmpty())
        {
            g_queryRingHead = NextTimerQuery(g_queryRingHead);
            g_queryRingCount--;
        }
    }

    static void SendGPUSyncPacket(void)
    {
        // This is used to compare our own monotonic clock to the GL one...
        // OVRMonitor use the difference between these two values to sync up GPU and CPU events.
        GPUClockSyncPacket clockSyncPacket;
        clockSyncPacket.timestampCPU = GetNanoseconds();
        glGetInteger64v_(GL_TIMESTAMP_EXT, (GLint64*)&clockSyncPacket.timestampGPU);
        //g_stream->WritePacket(clockSyncPacket); // TODO!!!!
    }

    static void InitTimerQuery(void)
    {
        const char *extensions = (const char*)glGetString(GL_EXTENSIONS);
        if(strstr(extensions, "GL_EXT_disjoint_timer_query"))
        {
            g_has_EXT_disjoint_timer_query = true;
            glGenQueriesEXT_          = (PFNGLGENQUERIESEXTPROC)         eglGetProcAddress("glGenQueriesEXT");
            glDeleteQueriesEXT_       = (PFNGLDELETEQUERIESEXTPROC)      eglGetProcAddress("glDeleteQueriesEXT");
            glIsQueryEXT_             = (PFNGLISQUERYEXTPROC)            eglGetProcAddress("glIsQueryEXT");
            glBeginQueryEXT_          = (PFNGLBEGINQUERYEXTPROC)         eglGetProcAddress("glBeginQueryEXT");
            glEndQueryEXT_            = (PFNGLENDQUERYEXTPROC)           eglGetProcAddress("glEndQueryEXT");
            glQueryCounterEXT_        = (PFNGLQUERYCOUNTEREXTPROC)       eglGetProcAddress("glQueryCounterEXT");
            glGetQueryivEXT_          = (PFNGLGETQUERYIVEXTPROC)         eglGetProcAddress("glGetQueryivEXT");
            glGetQueryObjectivEXT_    = (PFNGLGETQUERYOBJECTIVEXTPROC)   eglGetProcAddress("glGetQueryObjectivEXT");
            glGetQueryObjectuivEXT_   = (PFNGLGETQUERYOBJECTUIVEXTPROC)  eglGetProcAddress("glGetQueryObjectuivEXT");
            glGetQueryObjecti64vEXT_  = (PFNGLGETQUERYOBJECTI64VEXTPROC) eglGetProcAddress("glGetQueryObjecti64vEXT");
            glGetQueryObjectui64vEXT_ = (PFNGLGETQUERYOBJECTUI64VEXTPROC)eglGetProcAddress("glGetQueryObjectui64vEXT");
            glGetInteger64v_          = (PFNGLGETINTEGER64VPROC)         eglGetProcAddress("glGetInteger64v");
            for(GLuint i=0; i<g_queryRingSize; i++)
            {
                glGenQueriesEXT_(1, &g_queryRing[i].queryID);
            }
            SendGPUSyncPacket();
        }
        g_timerQueryInitialized = true;
    }

    static void ProcessTimerQueries(void)
    {
        if(IsQueryRingEmpty())
            return;

        do
        {
            while(true)
            {
                TimerQuery *query = QueryRingHead();
                // If no more queries are available, we are done...
                if(!query)
                    break;
                // Check to see if results are ready...
                GLint ready = GL_FALSE;
                glGetQueryObjectivEXT_(query->queryID, GL_QUERY_RESULT_AVAILABLE_EXT, &ready);
                // If results not ready stop trying to flush results as any remaining queries will also fail... we will try again later...
                if(!ready)
                    break;
                // Read timer value back...
                GLuint64 gputimestamp = 0;
                glGetQueryObjectui64vEXT_(query->queryID, GL_QUERY_RESULT, &gputimestamp);
                if(query->labelID)
                {
                    // Send our enter packet...
                    GPUZoneEnterPacket packet;
                    packet.labelID   = query->labelID;
                    packet.timestamp = gputimestamp;
                    AsyncStream::Acquire()->WritePacket(packet);
                }
                else
                {
                    // Send leave packet...
                    GPUZoneLeavePacket packet;
                    packet.timestamp = gputimestamp;
                    AsyncStream::Acquire()->WritePacket(packet);
                }
                // Remove this query from the ring buffer...
                QueryRingPop();
            }
        } while(IsQueryRingFull()); // if the ring is full, we keep trying until we have room for more events.

        // Check for disjoint errors...
        GLint disjointOccured = GL_FALSE;
        glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjointOccured);
        if(disjointOccured)
        {
            // TODO: send some sort of disjoint error packet so that OVRMonitor can mark it on the timeline.
            //       because this error likely coicides with bad GPU data and/or a major performance event.
            SendGPUSyncPacket();
        }
    }

    void EnterGPUZoneGLES3(const Label &label)
    {
        if(!CheckConnectionFlag(Enable_GPU_Zones))
            return;

        if(!g_timerQueryInitialized)
            InitTimerQuery();

        ProcessTimerQueries();

        if(g_has_EXT_disjoint_timer_query)
        {
            TimerQuery *query = QueryRingAlloc();
            OVR_CAPTURE_ASSERT(query);
            if(query)
            {
                glQueryCounterEXT_(query->queryID, GL_TIMESTAMP_EXT);
                query->labelID = label.GetIdentifier();
            }
        }
    }

    void LeaveGPUZoneGLES3(void)
    {
        if(!CheckConnectionFlag(Enable_GPU_Zones))
            return;

        OVR_CAPTURE_ASSERT(g_timerQueryInitialized);

        ProcessTimerQueries();
        
        if(g_has_EXT_disjoint_timer_query)
        {
            TimerQuery *query = QueryRingAlloc();
            OVR_CAPTURE_ASSERT(query);
            if(query)
            {
                glQueryCounterEXT_(query->queryID, GL_TIMESTAMP_EXT);
                query->labelID = 0;
            }
        }
    }

} // namespace Capture
} // namespace OVR

#endif // #if defined(OVR_CAPTURE_HAS_GLES3)
