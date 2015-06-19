// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

// handle differences between ES2 and full GL shaders
#if PLATFORM_USES_ES2
precision highp float;
#else
// #version 120 at the beginning is added in FSlateOpenGLShader::CompileShader()
#extension GL_EXT_gpu_shader4 : enable
#endif

// Shader types
#define ST_Default	0
#define ST_Border	1
#define ST_Font		2
#define ST_Line		3

// Draw effects
uniform bool EffectsDisabled;
uniform bool IgnoreTextureAlpha;

uniform vec4 MarginUVs;
uniform int ShaderType;
uniform sampler2D ElementTexture;

varying vec4 TexCoords;
varying vec4 ClipOriginAndPos;
varying vec4 ClipExtents;
varying vec4 Color;

vec4 GetFontElementColor()
{
	vec4 OutColor = Color;
#if PLATFORM_LINUX
	OutColor.a *= texture2D(ElementTexture, TexCoords.xy).r; // OpenGL 3.2+ uses Red for single channel textures
#else
	OutColor.a *= texture2D(ElementTexture, TexCoords.xy).a;
#endif

	return OutColor;
}

vec4 GetDefaultElementColor()
{
	vec4 OutColor = Color;

    vec4 TextureColor = texture2D(ElementTexture, TexCoords.xy*TexCoords.zw);
	if( IgnoreTextureAlpha )
    {
        TextureColor.a = 1.0;
    }
	OutColor *= TextureColor;

	return OutColor;
}

vec4 GetBorderElementColor()
{
	vec4 OutColor = Color;
	vec4 InTexCoords = TexCoords;
	vec2 NewUV;
	if( InTexCoords.z == 0.0 && InTexCoords.w == 0.0 )
	{
		NewUV = InTexCoords.xy;
	}
	else
	{
		vec2 MinUV;
		vec2 MaxUV;
	
		if( InTexCoords.z > 0.0 )
		{
			MinUV = vec2(MarginUVs.x,0.0);
			MaxUV = vec2(MarginUVs.y,1.0);
			InTexCoords.w = 1.0;
		}
		else
		{
			MinUV = vec2(0.0,MarginUVs.z);
			MaxUV = vec2(1.0,MarginUVs.w);
			InTexCoords.z = 1.0;
		}

		NewUV = InTexCoords.xy*InTexCoords.zw;
		NewUV = fract(NewUV);
		NewUV = mix(MinUV,MaxUV,NewUV);	

	}

	vec4 TextureColor = texture2D(ElementTexture, NewUV);
	if( IgnoreTextureAlpha )
	{
		TextureColor.a = 1.0;
	}
		
	OutColor *= TextureColor;

	return OutColor;
}

vec4 GetSplineElementColor()
{
	vec2 SSPosition = ClipOriginAndPos.zw;
	vec4 InTexCoords = TexCoords;
	float Width = MarginUVs.x;
	float Radius = MarginUVs.y;
	
	vec2 StartPos = InTexCoords.xy;
	vec2 EndPos = InTexCoords.zw;
	
	vec2 Diff = vec2( StartPos.y - EndPos.y, EndPos.x - StartPos.x ) ;
	
	float K = 2.0/( (2.0*Radius + Width)*sqrt( dot( Diff, Diff) ) );
	
	vec3 E0 = K*vec3( Diff.x, Diff.y, (StartPos.x*EndPos.y - EndPos.x*StartPos.y) );
	E0.z += 1.0;
	
	vec3 E1 = K*vec3( -Diff.x, -Diff.y, (EndPos.x*StartPos.y - StartPos.x*EndPos.y) );
	E1.z += 1.0;
	
	vec3 Pos = vec3(SSPosition.xy,1.0);
	
	vec2 Distance = vec2( dot(E0,Pos), dot(E1,Pos) );
	
	if( any( lessThan(Distance, vec2(0.0)) ) )
	{
		// using discard instead of clip because
		// apparently clipped pixels are written into the stencil buffer but discards are not
		discard;
	}
	
	
	vec4 InColor = Color;
	
	float Index = min(Distance.x,Distance.y);
	
	// Without this, the texture sample sometimes samples the next entry in the table.  Usually occurs when sampling the last entry in the table but instead
	// samples the first and we get white pixels
	const float HalfPixelOffset = 1.0/32.0;
	
	InColor.a *= smoothstep(0.3, 1.0f, Index);
	
	if( InColor.a < 0.05 )
	{
		discard;
	}
	
	return InColor;
}

void clip(vec4 ClipTest)
{
	if( any( lessThan(ClipTest, vec4(0,0,0,0) ) ) ) discard;
}

float cross(vec2 a, vec2 b)
{
	return a.x*b.y - a.y*b.x;
}

/**
 * Given a point p and a parallelogram defined by point a and vectors b and c, determines in p is inside the parallelogram. 
 * returns a 4-vector that can be used with the clip instruction.
 */
vec4 PointInParallelogram(vec2 p, vec2 a, vec4 bc)
{
	// unoptomized form:
	//vec2 o = p - a;
	//vec2 b = bc.xy;
	//vec2 c = bc.zw;
	//float d = cross(b, c);
	//float s = -cross(o, b) / d;
	//float t = cross(o, c) / d;
	// test for s and t between 0 and 1
	//return vec4(s, 1 - s, t, 1 - t);

	vec2 o = p - a;
	// precompute 1/d
	float invD = 1/cross(bc.xy, bc.zw);
	// Compute an optimized o x b and o x c, leveraging that b and c are in the same vector register already (and free swizzles):
	//   (o.x * b .y  - o.y * b .x, o.x *  c.y - o.y *  c.x) ==
	//   (o.x * bc.y  - o.y * bc.x, o.x * bc.w - o.y * bc.z) ==
	//    o.x * bc.yw - o.y * bc.xz
	vec2 st = (o.x * bc.yw - o.y * bc.xz) * vec2(-invD, invD);
	// test for s and t between 0 and 1
	return vec4(st, vec2(1,1) - st);
}

void main()
{
	// Clip pixels which are outside of the clipping rect
	vec2 ClipOrigin = ClipOriginAndPos.xy;
	vec2 WindowPos = ClipOriginAndPos.zw;
	vec4 ClipTest = PointInParallelogram(WindowPos, ClipOrigin, ClipExtents);
	
	clip(ClipTest);

	//vec4 OutColorTint = any(lessThan(ClipTest, vec4(0,0,0,0))) ? vec4(1, 0.5, 0.5, 0.5) : vec4(1, 1, 1, 1);

	vec4 OutColor;

	if( ShaderType == ST_Default )
	{
		OutColor = GetDefaultElementColor();
	}
	else if( ShaderType == ST_Border )
	{
		OutColor = GetBorderElementColor();
	}
	else if( ShaderType == ST_Font )
	{
		OutColor = GetFontElementColor();
	}
	else
	{
		OutColor = GetSplineElementColor();
	}
	
	// gamma correct
	#if PLATFORM_USES_ES2
		OutColor.rgb = sqrt( OutColor.rgb );
	#else
		OutColor.rgb = pow(OutColor.rgb, vec3(1.0/2.2));
	#endif

	if( EffectsDisabled )
	{
		//desaturate
		vec3 LumCoeffs = vec3( 0.3, 0.59, .11 );
		float Lum = dot( LumCoeffs, OutColor.rgb );
		OutColor.rgb = mix( OutColor.rgb, vec3(Lum,Lum,Lum), .8 );
	
		vec3 Grayish = vec3(0.4, 0.4, 0.4);
	
		OutColor.rgb = mix( OutColor.rgb, Grayish, clamp( distance( OutColor.rgb, Grayish ), 0.0, 0.8)  );
	}

	gl_FragColor = OutColor.bgra 
		//* OutColorTint
		;
}
