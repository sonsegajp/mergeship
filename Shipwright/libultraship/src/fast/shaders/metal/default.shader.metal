@prism(type='metal', name='Fast3D Metal Shader', version='1.0.0', description='Ported shader to prism', author='Emill & Prism Team')

#include <metal_stdlib>
using namespace metal;

// BEGIN VERTEX SHADER
struct FrameUniforms {
    int frameCount;
    float noiseScale;
};

struct DrawUniforms {
    int textureFiltering[6];
    @if(o_prim_depth)
    float prim_depth;
    @end
};

struct FragOut {
    float4 color [[color(0)]];
    @if(o_prim_depth)
    float depth [[depth(any)]];
    @end
};

struct Vertex {
    float4 position [[attribute(@{get_vertex_index()})]];
    @{update_floats(4)}
    @for(i in 0..2)
        @if(o_textures[i])
            float2 texCoord@{i} [[attribute(@{get_vertex_index()})]];
            @{update_floats(2)}
            @for(j in 0..2)
                @if(o_clamp[i][j])
                    @if(j == 0)
                        float texClampS@{i} [[attribute(@{get_vertex_index()})]];
                    @else
                        float texClampT@{i} [[attribute(@{get_vertex_index()})]];
                    @end
                    @{update_floats(1)}
                @end
            @end
        @end
    @end
    @if(o_fog)
        float4 fog [[attribute(@{get_vertex_index()})]];
        @{update_floats(4)}
    @end
    @if(o_grayscale)
        float4 grayscale [[attribute(@{get_vertex_index()})]];
        @{update_floats(4)}
    @end
    @for(i in 0..o_inputs)
        @if(o_alpha)
            float4 input@{i + 1} [[attribute(@{get_vertex_index()})]];
            @{update_floats(4)}
        @else
            float3 input@{i + 1} [[attribute(@{get_vertex_index()})]];
            @{update_floats(3)}
        @end
    @end
};

struct ProjectedVertex {
    @for(i in 0..2)
        @if(o_textures[i])
            float2 texCoord@{i};
            @for(j in 0..2)
                @if(o_clamp[i][j])
                    @if(j == 0)
                        float texClampS@{i};
                    @else
                        float texClampT@{i};
                    @end
                @end
            @end
        @end
    @end
    @if(o_fog)
        float4 fog;
    @end
    @if(o_grayscale)
        float4 grayscale;
    @end
    @for(i in 0..o_inputs)
        @if(o_alpha)
            float4 input@{i + 1};
        @else
            float3 input@{i + 1};
        @end
    @end
    float4 position [[position]];
};

vertex ProjectedVertex vertexShader(Vertex in [[stage_in]]) {
    ProjectedVertex out;
    @for(i in 0..2)
        @if(o_textures[i])
            out.texCoord@{i} = in.texCoord@{i};
            @for(j in 0..2)
                @if(o_clamp[i][j])
                    @if(j == 0)
                        out.texClampS@{i} = in.texClampS@{i};
                    @else
                        out.texClampT@{i} = in.texClampT@{i};
                    @end
                @end
            @end
        @end
    @end
    @if(o_fog)
        out.fog = in.fog;
    @end
    @if(o_grayscale)
        out.grayscale = in.grayscale;
    @end
    @for(i in 0..o_inputs)
         out.input@{i + 1} = in.input@{i + 1};
    @end
    out.position = in.position;
    return out;
}
// END - BEGIN FRAGMENT SHADER

float mod(float x, float y) {
    return float(x - y * floor(x / y));
}

float3 mod(float3 a, float3 b) {
    return float3(a.x - b.x * floor(a.x / b.x), a.y - b.y * floor(a.y / b.y), a.z - b.z * floor(a.z / b.z));
}

float4 mod(float4 a, float4 b) {
    return float4(a.x - b.x * floor(a.x / b.x), a.y - b.y * floor(a.y / b.y), a.z - b.z * floor(a.z / b.z), a.w - b.w * floor(a.w / b.w));
}

#define WRAP(x, low, high) mod((x)-(low), (high)-(low)) + (low)
#define TEX_OFFSET(tex, texSmplr, texCoord, off, texSize) tex.sample(texSmplr, texCoord - off / texSize)

float4 filter3point(thread const texture2d<float> tex, thread const sampler texSmplr, thread const float2& texCoord, thread const float2& texSize) {
    float2 offset = fract((texCoord * texSize) - float2(0.5));
    offset -= float2(step(1.0, offset.x + offset.y));
    float4 c0 = TEX_OFFSET(tex, texSmplr, texCoord, offset, texSize);
    float4 c1 = TEX_OFFSET(tex, texSmplr, texCoord, float2(offset.x - sign(offset.x), offset.y), texSize);
    float4 c2 = TEX_OFFSET(tex, texSmplr, texCoord, float2(offset.x, offset.y - sign(offset.y)), texSize);
    return c0 + abs(offset.x) * (c1 - c0) + abs(offset.y) * (c2 - c0);
}

float4 hookTexture2D(thread const texture2d<float> tex, thread const sampler texSmplr, thread const float2& uv, thread const float2& texSize, thread const int filtering) {
@if(o_three_point_filtering)
    if(filtering == @{FILTER_THREE_POINT}) {
        return filter3point(tex, texSmplr, uv, texSize);
    }
@end
    return tex.sample(texSmplr, uv);
}

float random(float3 value) {
    float random = dot(sin(value), float3(12.9898, 78.233, 37.719));
    return fract(sin(random) * 143758.5453);
}

// MergeShip2 render styles (RenderStyle in interpreter.h)
@if(o_dither)
    // 4x4 ordered dither threshold in [0, 1)
    float msBayer2(float2 a) { a = floor(a); return fract(a.x / 2.0 + a.y * a.y * 0.75); }
    float msBayer4(float2 a) { return msBayer2(0.5 * a) * 0.25 + msBayer2(a); }
@end

fragment FragOut fragmentShader(
    ProjectedVertex in [[stage_in]],
    constant FrameUniforms &frameUniforms [[buffer(0)]],
    constant DrawUniforms &drawUniforms [[buffer(1)]]
@if(o_textures[0])
    , texture2d<float> uTex0 [[texture(0)]], sampler uTex0Smplr [[sampler(0)]]
@end
@if(o_textures[1])
    , texture2d<float> uTex1 [[texture(1)]], sampler uTex1Smplr [[sampler(1)]]
@end
@if(o_masks[0])
    , texture2d<float> uTexMask0 [[texture(2)]]
@end
@if(o_masks[1])
    , texture2d<float> uTexMask1 [[texture(3)]]
@end
@if(o_blend[0])
    , texture2d<float> uTexBlend0 [[texture(4)]]
@end
@if(o_blend[1])
    , texture2d<float> uTexBlend1 [[texture(5)]]
@end
) {
    @if(o_toon)
        // MergeShip2 toon style: the vertex shade in four flat bands (its hue kept)
        {
            float msLight = max(in.input@{o_shade_slot}.x, max(in.input@{o_shade_slot}.y, in.input@{o_shade_slot}.z));
            @if(o_style == 14)
                // (a surface whose light is painted into its vertex colours, the rooms: finer bands, soft edges)
                float msStep = msLight * 6.0;
                float msEdge = 0.2;
                float msBand = max((floor(msStep) + smoothstep(0.5 - msEdge, 0.5 + msEdge, fract(msStep))) / 6.0, 0.08);
            @else
                float msStep = msLight * 4.0;
                float msEdge = fwidth(msStep) * 0.75 + 0.0001;
                float msBand = max((floor(msStep) + smoothstep(0.5 - msEdge, 0.5 + msEdge, fract(msStep))) / 4.0, 0.125);
            @end
            in.input@{o_shade_slot}.xyz *= msBand / max(msLight, 0.0001);
        }
    @end
    @for(i in 0..2)
        @if(o_textures[i])
            @{s = o_clamp[i][0]}
            @{t = o_clamp[i][1]}
            float2 texSize@{i} = float2(uTex@{i}.get_width(), uTex@{i}.get_height());
            @if(!s && !t)
                float2 vTexCoordAdj@{i} = in.texCoord@{i};
            @else
                @if(s && t)
                    float2 vTexCoordAdj@{i} = fast::clamp(in.texCoord@{i}, float2(0.5) / texSize@{i}, float2(in.texClampS@{i}, in.texClampT@{i}));
                @elseif(s)
                    float2 vTexCoordAdj@{i} = float2(fast::clamp(in.texCoord@{i}.x, 0.5 / texSize@{i}.x, in.texClampS@{i}), in.texCoord@{i}.y);
                @else
                    float2 vTexCoordAdj@{i} = float2(in.texCoord@{i}.x, fast::clamp(in.texCoord@{i}.y, 0.5 / texSize@{i}.y, in.texClampT@{i}));
                @end
            @end

            float4 texVal@{i} = hookTexture2D(uTex@{i}, uTex@{i}Smplr, vTexCoordAdj@{i}, texSize@{i}, drawUniforms.textureFiltering[@{i}]);

            @if(o_masks[i])
                float2 maskSize@{i} = float2(uTexMask@{i}.get_width(), uTexMask@{i}.get_height());
                float4 maskVal@{i} = hookTexture2D(uTexMask@{i}, uTex@{i}Smplr, vTexCoordAdj@{i}, maskSize@{i}, drawUniforms.textureFiltering[@{i}]);
                @if(o_blend[i])
                    float4 blendVal@{i} = hookTexture2D(uTexBlend@{i}, uTex@{i}Smplr, vTexCoordAdj@{i}, texSize@{i}, drawUniforms.textureFiltering[@{i}]);
                @else
                    float4 blendVal@{i} = float4(0, 0, 0, 0);
                @end

                texVal@{i} = mix(texVal@{i}, blendVal@{i}, maskVal@{i}.w);
            @end
        @end
    @end
    
    @if(o_alpha)
        float4 texel;
    @else
        float3 texel;
    @end

    @if(o_2cyc)
        @{f_range = 2}
    @else
        @{f_range = 1}
    @end

    @for(c in 0..f_range)
        @if(c == 1)
            @if(o_alpha)
                @if(o_c[c][1][2] == SHADER_COMBINED)
                    texel.w = WRAP(texel.w, -1.01, 1.01);
                @else
                    texel.w = WRAP(texel.w, -0.51, 1.51);
                @end
            @end

            @if(o_c[c][0][2] == SHADER_COMBINED)
                texel.xyz = WRAP(texel.xyz, -1.01, 1.01);
            @else
                texel.xyz = WRAP(texel.xyz, -0.51, 1.51);
            @end
        @end

        @if(!o_color_alpha_same[c] && o_alpha)
            texel = float4(@{
            append_formula(o_c[c], o_do_single[c][0],
                           o_do_multiply[c][0], o_do_mix[c][0], false, false, true, c == 0)
            }, @{append_formula(o_c[c], o_do_single[c][1],
                           o_do_multiply[c][1], o_do_mix[c][1], true, true, true, c == 0)
            });
        @else
            texel = @{append_formula(o_c[c], o_do_single[c][0],
                           o_do_multiply[c][0], o_do_mix[c][0], o_alpha, false,
                           o_alpha, c == 0)};
        @end
    @end

    @if(o_texture_edge && o_alpha)
        if (texel.w > 0.19) texel.w = 1.0; else discard_fragment();
    @end

    texel = WRAP(texel, -0.51, 1.51);
    texel = clamp(texel, 0.0, 1.0);
    // TODO discard if alpha is 0?

    @if(o_fog)
        @if(o_alpha)
            texel = float4(mix(texel.xyz, in.fog.xyz, in.fog.w), texel.w);
        @else
            texel = mix(texel, in.fog.xyz, in.fog.w);
        @end
    @end

    @if(o_grayscale)
        float intensity = (texel.x + texel.y + texel.z) / 3.0;
        float3 new_texel = in.grayscale.xyz * intensity;
        texel.xyz = mix(texel.xyz, new_texel, in.grayscale.w);
    @end

    // MergeShip2 render styles: the colour filters, on the finished colour
    @if(o_style == 5)
        texel.xyz = floor(texel.xyz * 31.0 + msBayer4(in.position.xy)) / 31.0;
    @end
    @if(o_style == 6)
        {
            // an 8-bit console's palette: twelve hues, grey or coloured, in six steps of brightness
            float msV = max(texel.x, max(texel.y, texel.z));
            float msD = msV - min(texel.x, min(texel.y, texel.z));
            float msS = msD / max(msV, 0.25);
            float msH = 0.0;
            if (msD > 0.0001) {
                if (msV == texel.x) {
                    msH = (texel.y - texel.z) / msD;
                } else if (msV == texel.y) {
                    msH = 2.0 + (texel.z - texel.x) / msD;
                } else {
                    msH = 4.0 + (texel.x - texel.y) / msD;
                }
                msH = fract(msH / 6.0);
            }
            msH = floor(msH * 12.0 + 0.5) / 12.0;
            msS = msS < 0.2 ? 0.0 : (msS < 0.55 ? 0.5 : 0.9);
            msV = min(floor(sqrt(msV) * 5.0 + 0.35 + 0.3 * msBayer4(in.position.xy)) / 5.0, 1.0);
            msV = msV * msV;
            float3 msK = clamp(abs(fract(float3(msH) + float3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
            texel.xyz = msV * mix(float3(1.0), msK, msS);
        }
    @end
    @if(o_style == 8)
        texel.xyz = float3(dot(texel.xyz, float3(0.299, 0.587, 0.114)));
    @end
    @if(o_style == 9)
        texel.xyz = min(float3(dot(texel.xyz, float3(0.393, 0.769, 0.189)), dot(texel.xyz, float3(0.349, 0.686, 0.168)),
                               dot(texel.xyz, float3(0.272, 0.534, 0.131))), float3(1.0));
    @end
    @if(o_style == 10)
        texel.xyz = float3(1.0) - texel.xyz;
    @end
    @if(o_style == 11)
        {
            float msL = dot(texel.xyz, float3(0.299, 0.587, 0.114));
            texel.xyz = float3(0.10, 1.0, 0.20) * min(msL * 1.5 + 0.04, 1.0) *
                        (0.82 + 0.18 * step(1.0, mod(in.position.y, 2.0)));
        }
    @end
    @if(o_style == 12)
        {
            float msT = dot(texel.xyz, float3(0.299, 0.587, 0.114)) * 4.0;
            float3 msC = mix(float3(0.0, 0.0, 0.2), float3(0.25, 0.0, 0.8), saturate(msT));
            msC = mix(msC, float3(0.9, 0.1, 0.25), saturate(msT - 1.0));
            msC = mix(msC, float3(1.0, 0.8, 0.0), saturate(msT - 2.0));
            texel.xyz = mix(msC, float3(1.0), saturate(msT - 3.0));
        }
    @end
    @if(o_style == 13)
        {
            float msQ = floor(dot(texel.xyz, float3(0.299, 0.587, 0.114)) * 3.0 + msBayer4(in.position.xy));
            float3 msC = mix(float3(0.06, 0.22, 0.06), float3(0.19, 0.38, 0.19), saturate(msQ));
            msC = mix(msC, float3(0.55, 0.67, 0.06), saturate(msQ - 1.0));
            texel.xyz = mix(msC, float3(0.61, 0.74, 0.06), saturate(msQ - 2.0));
        }
    @end

    @if(o_alpha && o_noise)
        float2 coords = in.position.xy * frameUniforms.noiseScale;
        texel.w *= round(saturate(random(float3(floor(coords), float(frameUniforms.frameCount))) + texel.w - 0.5));
    @end

    FragOut out;
    @if(o_alpha)
        @if(o_alpha_threshold)
            if (texel.w < 8.0 / 256.0) discard_fragment();
        @end
        @if(o_invisible)
            texel.w = 0.0;
        @end
        out.color = texel;
    @else
        out.color = float4(texel, 1.0);
    @end
    @if(o_prim_depth)
        out.depth = drawUniforms.prim_depth;
    @end
    return out;
}