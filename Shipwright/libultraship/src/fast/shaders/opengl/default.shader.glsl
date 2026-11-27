@prism(type='fragment', name='Fast3D Fragment Shader', version='1.0.0', description='Ported shader to prism', author='Emill & Prism Team')

@{GLSL_VERSION}

@if(VERTEX_SHADER)
    @{attr} vec4 aVtxPos;

    @for(i in 0..2)
        @if(o_textures[i])
            @{attr} vec2 aTexCoord@{i};
            @{out} vec2 vTexCoord@{i};
            @{update_floats(2)}
            @for(j in 0..2)
                @if(o_clamp[i][j])
                    @if(j == 0)
                        @{attr} float aTexClampS@{i};
                        @{out} float vTexClampS@{i};
                    @else
                        @{attr} float aTexClampT@{i};
                        @{out} float vTexClampT@{i};
                    @end
                    @{update_floats(1)}
                @end
            @end
        @end
    @end

    @if(o_fog)
        @{attr} vec4 aFog;
        @{out} vec4 vFog;
        @{update_floats(4)}
    @end

    @if(o_grayscale)
        @{attr} vec4 aGrayscaleColor;
        @{out} vec4 vGrayscaleColor;
        @{update_floats(4)}
    @end

    @for(i in 0..o_inputs)
        @if(o_alpha)
            @{attr} vec4 aInput@{i + 1};
            @{out} vec4 vInput@{i + 1};
            @{update_floats(4)}
        @else
            @{attr} vec3 aInput@{i + 1};
            @{out} vec3 vInput@{i + 1};
            @{update_floats(3)}
        @end
    @end

    void main() {
        @for(i in 0..2)
            @if(o_textures[i])
                vTexCoord@{i} = aTexCoord@{i};
                @for(j in 0..2)
                    @if(o_clamp[i][j])
                        @if(j == 0)
                            vTexClampS@{i} = aTexClampS@{i};
                        @else
                            vTexClampT@{i} = aTexClampT@{i};
                        @end
                    @end
                @end
            @end
        @end
        @if(o_fog)
            vFog = aFog;
        @end
        @if(o_grayscale)
            vGrayscaleColor = aGrayscaleColor;
        @end
        @for(i in 0..o_inputs)
            vInput@{i + 1} = aInput@{i + 1};
        @end
        gl_Position = aVtxPos;
        @if(opengles)
            gl_Position.z *= 0.3f;
        @end
    }
@else
    @if(core_opengl || opengles)
    out vec4 vOutColor;
    @end

    @for(i in 0..2)
        @if(o_textures[i])
            @{attr} vec2 vTexCoord@{i};
            @for(j in 0..2)
                @if(o_clamp[i][j])
                    @if(j == 0)
                        @{attr} float vTexClampS@{i};
                    @else
                        @{attr} float vTexClampT@{i};
                    @end
                @end
            @end
        @end
    @end

    @if(o_fog) @{attr} vec4 vFog;
    @if(o_grayscale) @{attr} vec4 vGrayscaleColor;

    @for(i in 0..o_inputs)
        @if(o_alpha)
            @{attr} vec4 vInput@{i + 1};
        @else
            @{attr} vec3 vInput@{i + 1};
        @end
    @end

    @if(o_textures[0]) uniform sampler2D uTex0;
    @if(o_textures[1]) uniform sampler2D uTex1;

    @if(o_masks[0]) uniform sampler2D uTexMask0;
    @if(o_masks[1]) uniform sampler2D uTexMask1;

    @if(o_blend[0]) uniform sampler2D uTexBlend0;
    @if(o_blend[1]) uniform sampler2D uTexBlend1;

    uniform int frame_count;
    uniform float noise_scale;

    @if(o_prim_depth)
    uniform float prim_depth;
    @end

    uniform int texture_width[2];
    uniform int texture_height[2];
    uniform int texture_filtering[2];

    #define TEX_OFFSET(off) @{texture}(tex, texCoord - off / texSize)
    #define WRAP(x, low, high) mod((x)-(low), (high)-(low)) + (low)

    float random(in vec3 value) {
        float random = dot(sin(value), vec3(12.9898, 78.233, 37.719));
        return fract(sin(random) * 143758.5453);
    }

    vec4 fromLinear(vec4 linearRGB){
        bvec3 cutoff = lessThan(linearRGB.rgb, vec3(0.0031308));
        vec3 higher = vec3(1.055)*pow(linearRGB.rgb, vec3(1.0/2.4)) - vec3(0.055);
        vec3 lower = linearRGB.rgb * vec3(12.92);
        return vec4(mix(higher, lower, cutoff), linearRGB.a);
    }

    vec4 filter3point(in sampler2D tex, in vec2 texCoord, in vec2 texSize) {
        vec2 offset = fract(texCoord*texSize - vec2(0.5));
        offset -= step(1.0, offset.x + offset.y);
        vec4 c0 = TEX_OFFSET(offset);
        vec4 c1 = TEX_OFFSET(vec2(offset.x - sign(offset.x), offset.y));
        vec4 c2 = TEX_OFFSET(vec2(offset.x, offset.y - sign(offset.y)));
        return c0 + abs(offset.x)*(c1-c0) + abs(offset.y)*(c2-c0);
    }

    vec4 hookTexture2D(in int id, sampler2D tex, in vec2 uv, in vec2 texSize) {
    @if(o_three_point_filtering)
        if(texture_filtering[id] == @{FILTER_THREE_POINT}) {
            return filter3point(tex, uv, texSize);
        }
    @end
        return @{texture}(tex, uv);
    }

    #define TEX_SIZE(tex) vec2(texture_width[tex], texture_height[tex])

    // MergeShip2 render styles (RenderStyle in interpreter.h)
    @if(o_toon)
        // Toon: the colour input that carries the vertex shade is read through msShade, which main() bands
        @if(o_alpha)
            vec4 msShadeIn() { return vInput@{o_shade_slot}; }
            vec4 msShade;
        @else
            vec3 msShadeIn() { return vInput@{o_shade_slot}; }
            vec3 msShade;
        @end
        #define vInput@{o_shade_slot} msShade
    @end
    @if(o_dither)
        // 4x4 ordered dither threshold in [0, 1)
        float msBayer2(vec2 a) { a = floor(a); return fract(a.x / 2.0 + a.y * a.y * 0.75); }
        float msBayer4(vec2 a) { return msBayer2(0.5 * a) * 0.25 + msBayer2(a); }
    @end

    void main() {
        @if(o_toon)
            msShade = msShadeIn();
            {
                float msLight = max(msShade.r, max(msShade.g, msShade.b));
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
                msShade.rgb *= msBand / max(msLight, 0.0001);
            }
        @end
        @for(i in 0..2)
            @if(o_textures[i])
                @{s = o_clamp[i][0]}
                @{t = o_clamp[i][1]}

                vec2 texSize@{i} = TEX_SIZE(@{i});

                @if(!s && !t)
                    vec2 vTexCoordAdj@{i} = vTexCoord@{i};
                @else
                    @if(s && t)
                        vec2 vTexCoordAdj@{i} = clamp(vTexCoord@{i}, 0.5 / texSize@{i}, vec2(vTexClampS@{i}, vTexClampT@{i}));
                    @elseif(s)
                        vec2 vTexCoordAdj@{i} = vec2(clamp(vTexCoord@{i}.s, 0.5 / texSize@{i}.s, vTexClampS@{i}), vTexCoord@{i}.t);
                    @else
                        vec2 vTexCoordAdj@{i} = vec2(vTexCoord@{i}.s, clamp(vTexCoord@{i}.t, 0.5 / texSize@{i}.t, vTexClampT@{i}));
                    @end
                @end

                vec4 texVal@{i} = hookTexture2D(@{i}, uTex@{i}, vTexCoordAdj@{i}, texSize@{i});

                @if(o_masks[i])
                    @if(opengles) 
                        vec2 maskSize@{i} = vec2(textureSize(uTexMask@{i}, 0));
                    @else 
                        vec2 maskSize@{i} = textureSize(uTexMask@{i}, 0);
                    @end

                    vec4 maskVal@{i} = hookTexture2D(@{i}, uTexMask@{i}, vTexCoordAdj@{i}, maskSize@{i});

                    @if(o_blend[i])
                        vec4 blendVal@{i} = hookTexture2D(@{i}, uTexBlend@{i}, vTexCoordAdj@{i}, texSize@{i});
                    @else
                        vec4 blendVal@{i} = vec4(0, 0, 0, 0);
                    @end

                    texVal@{i} = mix(texVal@{i}, blendVal@{i}, maskVal@{i}.a);
                @end
            @end
        @end

        @if(o_alpha) 
            vec4 texel;
        @else 
            vec3 texel;
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
                        texel.a = WRAP(texel.a, -1.01, 1.01);
                    @else
                        texel.a = WRAP(texel.a, -0.51, 1.51);
                    @end
                @end

                @if(o_c[c][0][2] == SHADER_COMBINED)
                    texel.rgb = WRAP(texel.rgb, -1.01, 1.01);
                @else
                    texel.rgb = WRAP(texel.rgb, -0.51, 1.51);
                @end
            @end

            @if(!o_color_alpha_same[c] && o_alpha)
                texel = vec4(@{
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

        texel = WRAP(texel, -0.51, 1.51);
        texel = clamp(texel, 0.0, 1.0);
        // TODO discard if alpha is 0?
        @if(o_fog)
            @if(o_alpha)
                texel = vec4(mix(texel.rgb, vFog.rgb, vFog.a), texel.a);
            @else
                texel = mix(texel, vFog.rgb, vFog.a);
            @end
        @end

        @if(o_texture_edge && o_alpha)
            if (texel.a > 0.19) texel.a = 1.0; else discard;
        @end

        @if(o_alpha && o_noise)
            texel.a *= floor(clamp(random(vec3(floor(gl_FragCoord.xy * noise_scale), float(frame_count))) + texel.a, 0.0, 1.0));
        @end

        @if(o_grayscale)
            float intensity = (texel.r + texel.g + texel.b) / 3.0;
            vec3 new_texel = vGrayscaleColor.rgb * intensity;
            texel.rgb = mix(texel.rgb, new_texel, vGrayscaleColor.a);
        @end

        // MergeShip2 render styles: the colour filters, on the finished colour
        @if(o_style == 5)
            texel.rgb = floor(texel.rgb * 31.0 + msBayer4(gl_FragCoord.xy)) / 31.0;
        @end
        @if(o_style == 6)
            {
                // an 8-bit console's palette: twelve hues, grey or coloured, in six steps of brightness
                float msV = max(texel.r, max(texel.g, texel.b));
                float msD = msV - min(texel.r, min(texel.g, texel.b));
                float msS = msD / max(msV, 0.25);
                float msH = 0.0;
                if (msD > 0.0001) {
                    if (msV == texel.r) {
                        msH = (texel.g - texel.b) / msD;
                    } else if (msV == texel.g) {
                        msH = 2.0 + (texel.b - texel.r) / msD;
                    } else {
                        msH = 4.0 + (texel.r - texel.g) / msD;
                    }
                    msH = fract(msH / 6.0);
                }
                msH = floor(msH * 12.0 + 0.5) / 12.0;
                msS = msS < 0.2 ? 0.0 : (msS < 0.55 ? 0.5 : 0.9);
                msV = min(floor(sqrt(msV) * 5.0 + 0.35 + 0.3 * msBayer4(gl_FragCoord.xy)) / 5.0, 1.0);
                msV = msV * msV;
                vec3 msK = clamp(abs(fract(vec3(msH) + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
                texel.rgb = msV * mix(vec3(1.0), msK, msS);
            }
        @end
        @if(o_style == 8)
            texel.rgb = vec3(dot(texel.rgb, vec3(0.299, 0.587, 0.114)));
        @end
        @if(o_style == 9)
            texel.rgb = min(vec3(dot(texel.rgb, vec3(0.393, 0.769, 0.189)), dot(texel.rgb, vec3(0.349, 0.686, 0.168)),
                                 dot(texel.rgb, vec3(0.272, 0.534, 0.131))), vec3(1.0));
        @end
        @if(o_style == 10)
            texel.rgb = vec3(1.0) - texel.rgb;
        @end
        @if(o_style == 11)
            {
                float msL = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
                texel.rgb = vec3(0.10, 1.0, 0.20) * min(msL * 1.5 + 0.04, 1.0) *
                            (0.82 + 0.18 * step(1.0, mod(gl_FragCoord.y, 2.0)));
            }
        @end
        @if(o_style == 12)
            {
                float msT = dot(texel.rgb, vec3(0.299, 0.587, 0.114)) * 4.0;
                vec3 msC = mix(vec3(0.0, 0.0, 0.2), vec3(0.25, 0.0, 0.8), clamp(msT, 0.0, 1.0));
                msC = mix(msC, vec3(0.9, 0.1, 0.25), clamp(msT - 1.0, 0.0, 1.0));
                msC = mix(msC, vec3(1.0, 0.8, 0.0), clamp(msT - 2.0, 0.0, 1.0));
                texel.rgb = mix(msC, vec3(1.0), clamp(msT - 3.0, 0.0, 1.0));
            }
        @end
        @if(o_style == 13)
            {
                float msQ = floor(dot(texel.rgb, vec3(0.299, 0.587, 0.114)) * 3.0 + msBayer4(gl_FragCoord.xy));
                vec3 msC = mix(vec3(0.06, 0.22, 0.06), vec3(0.19, 0.38, 0.19), clamp(msQ, 0.0, 1.0));
                msC = mix(msC, vec3(0.55, 0.67, 0.06), clamp(msQ - 1.0, 0.0, 1.0));
                texel.rgb = mix(msC, vec3(0.61, 0.74, 0.06), clamp(msQ - 2.0, 0.0, 1.0));
            }
        @end

        @if(o_alpha)
            @if(o_alpha_threshold)
                if (texel.a < 8.0 / 256.0) discard;
            @end
            @if(o_invisible)
                texel.a = 0.0;
            @end
            @{vOutColor} = texel;
        @else
            @{vOutColor} = vec4(texel, 1.0);
        @end

        @if(srgb_mode)
            @{vOutColor} = fromLinear(@{vOutColor});
        @end

        @if(o_prim_depth)
            gl_FragDepth = prim_depth;
        @end
    }
@end