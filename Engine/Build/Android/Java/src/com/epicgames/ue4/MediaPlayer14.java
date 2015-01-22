// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

package com.epicgames.ue4;

import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.graphics.Rect;
import android.opengl.GLES20;
import android.opengl.GLES11Ext;
import android.opengl.Matrix;
import android.os.Build;
import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;

/*
	Custom media player that renders video to a frame buffer. This
	variation is for API 14 and above.
*/
public class MediaPlayer14
	extends android.media.MediaPlayer
{
	private boolean SwizzlePixels = true;
	private volatile boolean WaitOnBitmapRender = false;

	public MediaPlayer14()
	{
		SwizzlePixels = true;
		WaitOnBitmapRender = false;
	}

	public MediaPlayer14(boolean swizzlePixels)
	{
		SwizzlePixels = swizzlePixels;
		WaitOnBitmapRender = false;
	}

	public boolean setDataSource(
		String moviePath, long offset, long size)
		throws IOException,
			java.lang.InterruptedException,
			java.util.concurrent.ExecutionException
	{
		try
		{
			File f = new File(moviePath);
			if (!f.exists() || !f.isFile()) 
			{
				return false;
			}
			RandomAccessFile data = new RandomAccessFile(f, "r");
			setDataSource(data.getFD(), offset, size);
			if (!CreateBitmapRenderer())
			{
				reset();
				return false;
			}
		}
		catch(IOException e)
		{
			return false;
		}
		return true;
	}
	
	public boolean setDataSource(
		AssetManager assetManager, String assetPath, long offset, long size)
		throws java.lang.InterruptedException,
			java.util.concurrent.ExecutionException
	{
		try
		{
			AssetFileDescriptor assetFD = assetManager.openFd(assetPath);
			setDataSource(assetFD.getFileDescriptor(), offset, size);
			if (!CreateBitmapRenderer())
			{
				reset();
				return false;
			}
		}
		catch(IOException e)
		{
			return false;
		}
		return true;
	}

	private boolean mVideoEnabled = true;
	
	public void setVideoEnabled(boolean enabled)
	{
		WaitOnBitmapRender = true;

		mVideoEnabled = enabled;
		if (mVideoEnabled && null != mBitmapRenderer.getSurface())
		{
			setSurface(mBitmapRenderer.getSurface());
		}
		else
		{
			setSurface(null);
		}

		WaitOnBitmapRender = false;
	}
	
	public void setAudioEnabled(boolean enabled)
	{
		if (enabled)
		{
			setVolume(1,1);
		}
		else
		{
			setVolume(0,0);
		}
	}
	
	public java.nio.Buffer getVideoLastFrameData()
	{

		if (null != mBitmapRenderer)
		{
			WaitOnBitmapRender = true;
			java.nio.Buffer data = mBitmapRenderer.updateFrameData();
			WaitOnBitmapRender = false;
			return data;
		}
		else
		{
			return null;
		}
	}

	public boolean getVideoLastFrame(int destTexture)
	{
		if (null != mBitmapRenderer)
		{
			WaitOnBitmapRender = true;
			boolean result = mBitmapRenderer.updateFrameData(destTexture);
			WaitOnBitmapRender = false;
			return result;
		}
		else
		{
			return false;
		}
	}

	public void release()
	{
		if (null != mBitmapRenderer)
		{
			while (WaitOnBitmapRender) ;
			mBitmapRenderer.release();
			mBitmapRenderer = null;
		}
		super.release();
	}

	public void reset()
	{
		if (null != mBitmapRenderer)
		{
			while (WaitOnBitmapRender) ;
			mBitmapRenderer.release();
			mBitmapRenderer = null;
		}
		super.reset();
	}
	
	/*
		All this internal surface view does is manage the
		offscreen bitmap that the media player decoding can
		render into for eventual extraction to the UE4 buffers.
	*/
	class BitmapRenderer
		implements android.graphics.SurfaceTexture.OnFrameAvailableListener
	{
		private java.nio.Buffer mFrameData = null;
		private int mLastFramePosition = -1;
		private android.graphics.SurfaceTexture mSurfaceTexture = null;
		private int mTextureWidth = -1;
		private int mTextureHeight = -1;
		private android.view.Surface mSurface = null;
		private boolean mFrameAvailable = false;
		private int mTextureID = -1;
		private int mFBO = -1;
		private int mBlitVertexShaderID = -1;
		private int mBlitFragmentShaderID = -1;

		private int GL_TEXTURE_EXTERNAL_OES = 0x8D65;
		
		public BitmapRenderer()
		{
			initSurfaceTexture();
		}

		private void initSurfaceTexture()
		{
			int[] textures = new int[1];
			GLES20.glGenTextures(1, textures, 0);
			mTextureID = textures[0];
			if (mTextureID <= 0)
			{
				release();
				return;
			}
			mSurfaceTexture = new android.graphics.SurfaceTexture(mTextureID);
			mSurfaceTexture.setOnFrameAvailableListener(this);
			mSurface = new android.view.Surface(mSurfaceTexture);

			int[] glInt = new int[1];

			GLES20.glGenFramebuffers(1,glInt,0);
			mFBO = glInt[0];
			if (mFBO <= 0)
			{
				release();
				return;
			}

			// Special shaders for blit of movie texture.
            mBlitVertexShaderID = createShader(GLES20.GL_VERTEX_SHADER, mBlitVextexShader);
            if (mBlitVertexShaderID == 0)
			{
				release();
				return;
			}
            int mBlitFragmentShaderID = createShader(GLES20.GL_FRAGMENT_SHADER,
				SwizzlePixels ? mBlitFragmentShaderBGRA : mBlitFragmentShaderRGBA);
            if (mBlitFragmentShaderID == 0)
			{
				release();
				return;
			}
            mProgram = GLES20.glCreateProgram();
			if (mProgram <= 0)
			{
				release();
				return;
			}
            GLES20.glAttachShader(mProgram, mBlitVertexShaderID);
            GLES20.glAttachShader(mProgram, mBlitFragmentShaderID);
            GLES20.glLinkProgram(mProgram);
            int[] linkStatus = new int[1];
            GLES20.glGetProgramiv(mProgram, GLES20.GL_LINK_STATUS, linkStatus, 0);
            if (linkStatus[0] != GLES20.GL_TRUE)
			{
                GameActivity.Log.error("Could not link program: ");
                GameActivity.Log.error(GLES20.glGetProgramInfoLog(mProgram));
                GLES20.glDeleteProgram(mProgram);
                mProgram = 0;
				return;
            }
            mPositionAttrib = GLES20.glGetAttribLocation(mProgram, "Position");
            mTexCoordsAttrib = GLES20.glGetAttribLocation(mProgram, "TexCoords");

			// Create blit mesh.
            mTriangleVertices = java.nio.ByteBuffer.allocateDirect(
                mTriangleVerticesData.length * FLOAT_SIZE_BYTES)
                    .order(java.nio.ByteOrder.nativeOrder()).asFloatBuffer();
            mTriangleVertices.put(mTriangleVerticesData).position(0);
			GLES20.glGenBuffers(1,glInt,0);
			mBlitBuffer = glInt[0];
			if (mBlitBuffer <= 0)
			{
				release();
				return;
			}
			GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, mBlitBuffer);
			GLES20.glBufferData(GLES20.GL_ARRAY_BUFFER,
				mTriangleVerticesData.length*FLOAT_SIZE_BYTES,
				mTriangleVertices, GLES20.GL_STATIC_DRAW);
		}

		public boolean isValid()
		{
			return mSurfaceTexture != null;
		}

        private int createShader(int shaderType, String source)
		{
            int shader = GLES20.glCreateShader(shaderType);
            if (shader != 0)
			{
                GLES20.glShaderSource(shader, source);
                GLES20.glCompileShader(shader);
                int[] compiled = new int[1];
                GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, compiled, 0);
                if (compiled[0] == 0)
				{
                    GameActivity.Log.error("Could not compile shader " + shaderType + ":");
                    GameActivity.Log.error(GLES20.glGetShaderInfoLog(shader));
                    GLES20.glDeleteShader(shader);
                    shader = 0;
                }
            }
            return shader;
        }

		public void onFrameAvailable(android.graphics.SurfaceTexture st)
		{
			synchronized(this)
			{
				mFrameAvailable = true;
			}
		}

		public android.graphics.SurfaceTexture getSurfaceTexture()
		{
			return mSurfaceTexture;
		}

		public android.view.Surface getSurface()
		{
			return mSurface;
		}

		// NOTE: Synchronized with updateFrameData to prevent frame
		// updates while the surface may need to get reallocated.
		public void setSize(int width, int height)
		{
			synchronized(this)
			{
				if (width != mTextureWidth ||
					height != mTextureHeight)
				{
					mTextureWidth = width;
					mTextureHeight = height;
					mFrameData = null;
				}
			}
		}

		private static final int FLOAT_SIZE_BYTES = 4;
		private static final int TRIANGLE_VERTICES_DATA_STRIDE_BYTES = 4 * FLOAT_SIZE_BYTES;
		private static final int TRIANGLE_VERTICES_DATA_POS_OFFSET = 0;
		private static final int TRIANGLE_VERTICES_DATA_UV_OFFSET = 2;
		private final float[] mTriangleVerticesData = {
			// X, Y, U, V
			-1.0f, -1.0f, 0.f, 0.f,
			1.0f, -1.0f, 1.f, 0.f,
			-1.0f, 1.0f, 0.f, 1.f,
			1.0f, 1.0f, 1.f, 1.f,
			};

		private java.nio.FloatBuffer mTriangleVertices;

		private final String mBlitVextexShader =
			"attribute vec2 Position;\n" +
			"attribute vec2 TexCoords;\n" +
			"varying vec2 TexCoord;\n" +
			"void main() {\n" +
			"	TexCoord = TexCoords;\n" +
			"	gl_Position = vec4(Position, 0.0, 1.0);\n" +
			"}\n";

		// NOTE: We read the fragment as BGRA so that in the end, when
		// we glReadPixels out of the FBO, we get them in that order
		// and avoid having to swizzle the pixels in the CPU.
		private final String mBlitFragmentShaderBGRA =
			"#extension GL_OES_EGL_image_external : require\n" +
			"uniform samplerExternalOES VideoTexture;\n" +
			"varying highp vec2 TexCoord;\n" +
			"void main()\n" +
			"{\n" +
			"	gl_FragColor = texture2D(VideoTexture, TexCoord).bgra;\n" +
			"}\n";
		private final String mBlitFragmentShaderRGBA =
			"#extension GL_OES_EGL_image_external : require\n" +
			"uniform samplerExternalOES VideoTexture;\n" +
			"varying highp vec2 TexCoord;\n" +
			"void main()\n" +
			"{\n" +
			"	gl_FragColor = texture2D(VideoTexture, TexCoord).rgba;\n" +
			"}\n";

		private int mProgram;
		private int mPositionAttrib;
		private int mTexCoordsAttrib;
		private int mBlitBuffer;

		public java.nio.Buffer updateFrameData()
		{
			synchronized(this)
			{
				if (null == mFrameData && mTextureWidth > 0 && mTextureHeight > 0)
				{
					mFrameData = java.nio.ByteBuffer.allocateDirect(mTextureWidth*mTextureHeight*4);
				}
				if (!updateFrameTexture())
				{
					return null;
				}
				if (null != mSurfaceTexture)
				{
					// Copy surface texture to frame data.
					copyFrameTexture(0, mFrameData);
				}
			}
			return mFrameData;
		}

		public boolean updateFrameData(int destTexture)
		{
			synchronized(this)
			{
				if (!updateFrameTexture())
				{
					return false;
				}
				// Copy surface texture to destination texture.
				copyFrameTexture(destTexture, null);
			}
			return true;
		}

		private boolean updateFrameTexture()
		{
			if (!mFrameAvailable)
			{
				// We only return fresh data when we generate it. At other
				// time we return nothing to indicate that there was nothing
				// new to return. The media player deals with this by keeping
				// the last frame around and using that for rendering.
				return false;
			}
			mFrameAvailable = false;
			int current_frame_position = getCurrentPosition();
			mLastFramePosition = current_frame_position;
			if (null == mSurfaceTexture)
			{
				// Can't update if there's no surface to update into.
				return false;
			}

			// Clear gl errors as they can creap in from the UE4 renderer.
			GLES20.glGetError();
			// Get the latest video texture frame.
			mSurfaceTexture.updateTexImage();
			return true;
		}

		// Copy the surface texture to another texture, or to raw data. Note,
		// copying to raw data creates a temporary FBO texture.
		private void copyFrameTexture(int destTexture, java.nio.Buffer destData)
		{
			int[] glInt = new int[1];
			boolean[] glBool = new boolean[1];

			if (null != destData)
			{
				// Rewind data so that we can write to it.
				destData.position(0);
			}

			// Save and reset state.
			boolean previousBlend = GLES20.glIsEnabled(GLES20.GL_BLEND);
			boolean previousCullFace = GLES20.glIsEnabled(GLES20.GL_CULL_FACE);
			boolean previousScissorTest = GLES20.glIsEnabled(GLES20.GL_SCISSOR_TEST);
			boolean previousStencilTest = GLES20.glIsEnabled(GLES20.GL_STENCIL_TEST);
			boolean previousDepthTest = GLES20.glIsEnabled(GLES20.GL_DEPTH_TEST);
			boolean previousDither = GLES20.glIsEnabled(GLES20.GL_DITHER);
			GLES20.glGetIntegerv(GLES20.GL_FRAMEBUFFER_BINDING, glInt, 0);
			int previousFBO = glInt[0];
			int[] previousViewport = new int[4];
			GLES20.glGetIntegerv(GLES20.GL_VIEWPORT, previousViewport, 0);

			glVerify("save state");

			GLES20.glDisable(GLES20.GL_BLEND);
			GLES20.glDisable(GLES20.GL_CULL_FACE);
			GLES20.glDisable(GLES20.GL_SCISSOR_TEST);
			GLES20.glDisable(GLES20.GL_STENCIL_TEST);
			GLES20.glDisable(GLES20.GL_DEPTH_TEST);
			GLES20.glDisable(GLES20.GL_DITHER);
			GLES20.glColorMask(true,true,true,true);
			GLES20.glViewport(0, 0, mTextureWidth, mTextureHeight);

			glVerify("reset state");

			// Set-up FBO target texture..
			int FBOTextureID = 0;
			if (null != destData)
			{
				// Create temporary FBO for data copy.
				GLES20.glGenTextures(1,glInt,0);
				FBOTextureID = glInt[0];
			}
			else
			{
				// Use the given texture as the FBO.
				FBOTextureID = destTexture;
			}
			// Set the FBO to draw into the texture one-to-one.
			GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
			GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, FBOTextureID);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
				GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
				GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_NEAREST);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
				GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
				GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
			// Create the temp FBO data if needed.
			if (null != destData)
			{
				//int w = 1<<(32-Integer.numberOfLeadingZeros(mTextureWidth-1));
				//int h = 1<<(32-Integer.numberOfLeadingZeros(mTextureHeight-1));
				GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0,
					GLES20.GL_RGBA,
					mTextureWidth, mTextureHeight,
					0, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);
			}

			glVerify("set-up FBO texture");

			// Set to render to the FBO.
			GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, mFBO);

			GLES20.glFramebufferTexture2D(
				GLES20.GL_FRAMEBUFFER,
				GLES20.GL_COLOR_ATTACHMENT0,
				GLES20.GL_TEXTURE_2D, FBOTextureID, 0);

			// The special shaders to render from the video texture.
			GLES20.glUseProgram(mProgram);

			// Set the mesh that renders the video texture.
			GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, mBlitBuffer);
			GLES20.glEnableVertexAttribArray(mPositionAttrib);
			GLES20.glVertexAttribPointer(mPositionAttrib, 2, GLES20.GL_FLOAT, false,
				TRIANGLE_VERTICES_DATA_STRIDE_BYTES, 0);
			GLES20.glEnableVertexAttribArray(mTexCoordsAttrib);
			GLES20.glVertexAttribPointer(mTexCoordsAttrib, 2, GLES20.GL_FLOAT, false,
				TRIANGLE_VERTICES_DATA_STRIDE_BYTES,
				TRIANGLE_VERTICES_DATA_UV_OFFSET*FLOAT_SIZE_BYTES);

			glVerify("setup movie texture read");

			// Draw the video texture mesh.
			GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

			GLES20.glFinish();

			// Read the FBO texture pixels into raw data.
			if (null != destData)
			{
				GLES20.glReadPixels(
					0, 0, mTextureWidth, mTextureHeight,
					GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE,
					destData);
			}

			glVerify("draw & read movie texture");

			// Restore state and cleanup.
			if (previousFBO > 0)
			{
				GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, previousFBO);
			}
			if (null != destData && FBOTextureID > 0)
			{
				glInt[0] = FBOTextureID;
				GLES20.glDeleteTextures(1, glInt, 0);
			}
			GLES20.glViewport(previousViewport[0], previousViewport[1],
				previousViewport[2], previousViewport[3]);
			if (previousBlend) GLES20.glEnable(GLES20.GL_BLEND);
			if (previousCullFace) GLES20.glEnable(GLES20.GL_CULL_FACE);
			if (previousScissorTest) GLES20.glEnable(GLES20.GL_SCISSOR_TEST);
			if (previousStencilTest) GLES20.glEnable(GLES20.GL_STENCIL_TEST);
			if (previousDepthTest) GLES20.glEnable(GLES20.GL_DEPTH_TEST);
			if (previousDither) GLES20.glEnable(GLES20.GL_DITHER);
		}

        private void glVerify(String op)
		{
            int error;
            while ((error = GLES20.glGetError()) != GLES20.GL_NO_ERROR)
			{
                GameActivity.Log.error("MediaPlayer$BitmapRenderer: " + op + ": glGetError " + error);
                throw new RuntimeException(op + ": glGetError " + error);
            }
        }


        private void glWarn(String op)
		{
            int error;
            while ((error = GLES20.glGetError()) != GLES20.GL_NO_ERROR)
			{
                GameActivity.Log.warn("MediaPlayer$BitmapRenderer: " + op + ": glGetError " + error);
            }
        }

		public void release()
		{
			if (null != mSurface)
			{
				mSurface.release();
				mSurface = null;
			}
			if (null != mSurfaceTexture)
			{
				mSurfaceTexture.release();
				mSurfaceTexture = null;
			}
			int[] glInt = new int[1];
			if (mBlitBuffer > 0)
			{
				glInt[0] = mBlitBuffer;
				GLES20.glDeleteBuffers(1,glInt,0);
				mBlitBuffer = -1;
			}
			if (mProgram > 0)
			{
				GLES20.glDeleteProgram(mProgram);
				mProgram = -1;
			}
			if (mBlitVertexShaderID > 0)
			{
				GLES20.glDeleteShader(mBlitVertexShaderID);
				mBlitVertexShaderID = -1;
			}
			if (mBlitFragmentShaderID > 0)
			{
				GLES20.glDeleteShader(mBlitFragmentShaderID);
				mBlitFragmentShaderID = -1;
			}
			if (mFBO > 0)
			{
				glInt[0] = mFBO;
				GLES20.glDeleteFramebuffers(1,glInt,0);
				mFBO = -1;
			}
			if (mTextureID > 0)
			{
				glInt[0] = mTextureID;
				GLES20.glDeleteTextures(1,glInt,0);
				mTextureID = -1;
			}
		}
	};

	private boolean CreateBitmapRenderer()
	{
		if (null != mBitmapRenderer)
		{
			mBitmapRenderer.release();
			mBitmapRenderer = null;
		}
		mBitmapRenderer = new BitmapRenderer();
		if (!mBitmapRenderer.isValid())
		{
			mBitmapRenderer = null;
			return false;
		}
		setOnVideoSizeChangedListener(new android.media.MediaPlayer.OnVideoSizeChangedListener() {
			public void onVideoSizeChanged(android.media.MediaPlayer player, int w, int h)
			{
				mBitmapRenderer.setSize(w,h);
			}
			});
		setVideoEnabled(true);
		setAudioEnabled(true);
		return true;
	}

	private BitmapRenderer mBitmapRenderer = null;
}
