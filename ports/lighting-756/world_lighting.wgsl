// View-space lighting. Game depth is converted with the current GX projection.
struct Lamp { positionRadius: vec4f, color: vec4f }
struct Bounce { positionRadius: vec4f, color: vec4f, normal: vec4f }
struct Frame {
    projection: vec4f, depth: vec4f, upEye: vec4f, sun: vec4f,
    fog: vec4f, sky: vec4f, amounts: vec4f, atmosphere: vec4f,
    worldX: vec4f, worldZ: vec4f, grid: vec4f, quality: vec4f,
    lamps: array<Lamp, 8>, water: array<vec4f, 4>, bounces: array<Bounce, 8>
}
@group(0) @binding(0) var<uniform> f: Frame;
@group(0) @binding(1) var scene: texture_2d<f32>;
@group(0) @binding(2) var depthTex: texture_2d<f32>;
@group(0) @binding(3) var illumination: texture_2d<f32>;

struct Vertex { @builtin(position) pos: vec4f, @location(0) uv: vec2f }
@vertex fn vs(@builtin(vertex_index) i: u32) -> Vertex {
    let xy = vec2f(f32((i << 1u) & 2u), f32(i & 2u));
    return Vertex(vec4f(xy * vec2f(2,-2) + vec2f(-1,1), 0, 1), xy);
}
fn bounded(uv: vec2f) -> bool { return all(uv > vec2f(0)) && all(uv < vec2f(1)); }
fn pixel(uv: vec2f) -> vec2i {
    let size = vec2i(textureDimensions(depthTex));
    return clamp(vec2i(uv * vec2f(size)), vec2i(0), size - 1);
}
fn distanceAt(uv: vec2f) -> f32 {
    let raw = textureLoad(depthTex, pixel(uv), 0).r;
    return clamp(abs(f.depth.y) / max(abs(raw + f.depth.x), 0.000001), f.depth.z, f.depth.w);
}
fn ray(uv: vec2f) -> vec3f {
    return vec3f((uv * vec2f(2,-2) + vec2f(-1,1) + f.projection.zw) / f.projection.xy, -1);
}
fn project(p: vec3f) -> vec2f {
    return (p.xy * f.projection.xy / max(-p.z, 0.001) - f.projection.zw) * vec2f(.5,-.5) + .5;
}
fn colorAt(uv: vec2f) -> vec3f { return textureLoad(scene, pixel(uv), 0).rgb; }
fn viewAt(uv: vec2f) -> vec3f { return ray(uv) * distanceAt(uv); }
fn surfaceNormal(uv: vec2f, p: vec3f) -> vec3f {
    let step = 1.0 / vec2f(textureDimensions(depthTex));
    let dx = vec2f(step.x, 0); let dy = vec2f(0, step.y);
    let l = p - viewAt(uv-dx); let r = viewAt(uv+dx)-p;
    let a = p - viewAt(uv-dy); let b = viewAt(uv+dy)-p;
    var n = cross(select(b,a,abs(a.z)<abs(b.z)), select(r,l,abs(l.z)<abs(r.z)));
    n *= inverseSqrt(max(dot(n,n), 1e-12));
    return select(-n,n,dot(n,-p)>=0);
}
fn luma(c: vec3f) -> f32 { return dot(c,vec3f(.2126,.7152,.0722)); }
fn linear(c: vec3f) -> vec3f { return c*c; }
fn waterHeight(p: vec3f) -> f32 {
    let xz = vec2f(dot(f.worldX.xyz,p)+f.worldX.w,dot(f.worldZ.xyz,p)+f.worldZ.w);
    let coord = (xz-f.grid.xy)/f.grid.z;
    if any(coord < vec2f(0)) || any(coord >= vec2f(3)) { return -1e7; }
    let cell = vec2u(coord);
    let heights = vec4f(f.water[cell.y][cell.x], f.water[cell.y][cell.x+1u],
                        f.water[cell.y+1u][cell.x], f.water[cell.y+1u][cell.x+1u]);
    // Near banks, mixed collision samples belong to different surfaces.
    if max(max(heights.x,heights.y),max(heights.z,heights.w)) -
       min(min(heights.x,heights.y),min(heights.z,heights.w)) > 12.0 { return -1e7; }
    return dot(heights,vec4f(.25));
}
fn reflectedColor(p: vec3f, normal: vec3f) -> vec4f {
    let dir = reflect(normalize(p),normal);
    let stride = clamp(-p.z*.016,8.,48.);
    var previous = -1e8;
    for(var k=1u;k<=u32(f.quality.y);k++) {
        let t = f32(k);
        let q = p+normal*3.+dir*(stride*t*(1.+.15*t));
        if q.z >= -f.depth.z { break; }
        let uv=project(q);
        if !bounded(uv) { break; }
        let gap = -q.z-distanceAt(uv);
        if previous < 0 && gap >= 0 && gap < stride*(1.+.5*t) {
            let edge=min(uv,1.-uv);
            return vec4f(linear(colorAt(uv)),smoothstep(0.,.08,min(edge.x,edge.y)));
        }
        previous=gap;
    }
    return vec4f(0);
}
fn visibilityTo(p: vec3f, q: vec3f) -> f32 {
    for(var i=1u;i<=3u;i++) {
        let s=mix(p,q,f32(i)*.22);
        if s.z >= -f.depth.z { break; }
        let uv=project(s);
        if !bounded(uv) { continue; }
        let gap = -s.z-distanceAt(uv);
        if gap > max(5.,-s.z*.006) && gap < 65. { return .25; }
    }
    return 1.;
}
@fragment fn gather(v: Vertex) -> @location(0) vec4f {
    let uv=v.uv; let d=distanceAt(uv); let p=ray(uv)*d;
    let base=linear(colorAt(uv)); var radiance=base;
    let skyPixel=d>=f.depth.w*.997;
    if !skyPixel {
        let n=surfaceNormal(uv,p);
        let up=clamp(dot(n,f.upEye.xyz),0.,1.);
        if f.amounts.x > 0. {
            var incoming = linear(f.sky.rgb)*(.045+.09*up);
            for(var i=0u;i<u32(f.quality.x);i++) {
                let angle=f32(i)*2.399963;
                let radius=clamp(95./d,.003,.09)*sqrt((f32(i)+.5)/f.quality.x);
                let sampleUV=uv+vec2f(cos(angle)*f.projection.x/f.projection.y,sin(angle))*radius;
                if !bounded(sampleUV) {continue;}
                let delta=viewAt(sampleUV)-p; let dist=length(delta);
                let facing=max(dot(n,delta/max(dist,.01)),0.);
                incoming+=linear(colorAt(sampleUV))*facing*(1.-smoothstep(15.,220.,dist))*1.8/f.quality.x;
            }
            for(var i=0u;i<8u;i++) {
                let hit=f.bounces[i];
                if hit.positionRadius.w<=0. {continue;}
                let delta=hit.positionRadius.xyz-p; let dist=length(delta);
                let dir=delta/max(dist,.01);
                let falloff=max(1.-dist/hit.positionRadius.w,0.);
                let outgoing=max(dot(hit.normal.xyz,-dir),0.);
                let receiver=max(dot(n,dir),0.);
                incoming+=hit.color.rgb*falloff*falloff*outgoing*receiver*1.6;
            }
            radiance+=f.amounts.x*(base+.045)*incoming;
        }
        if f.amounts.y > 0. {
            for(var i=0u;i<u32(f.grid.w);i++) {
                let lamp=f.lamps[i]; let delta=lamp.positionRadius.xyz-p; let dist=length(delta);
                let falloff=max(1.-dist/lamp.positionRadius.w,0.);
                if falloff>0. {
                    let ndotl=max(dot(n,delta/max(dist,.01)),0.);
                    var vis=1.;
                    if f.atmosphere.w>0. {vis=mix(1.,visibilityTo(p+n*3.,lamp.positionRadius.xyz),min(f.atmosphere.w,1.));}
                    radiance+=f.amounts.y*(base+.07)*lamp.color.rgb*(.16+.84*ndotl)*falloff*falloff*vis;
                }
            }
        }
        if f.atmosphere.w > 0. && f.sun.w>.01 && dot(n,normalize(f.sun.xyz))>0. {
            let vis=visibilityTo(p+n*3.,p+normalize(f.sun.xyz)*125.);
            radiance*=1.-(1.-vis)*min(f.atmosphere.w,.9)*f.sun.w*.52;
        }
        if f.amounts.z>0. {
            var origin=p; var reflectionNormal=n;
            var wet=f.sky.w*up*up*.30;
            let eyeRay=ray(uv); let down=dot(eyeRay,f.upEye.xyz);
            if f.quality.z>0. && down < -.001 {
                // Use the player's/camera's nearby water elevation only as a first guess.
                // Confirm the intersection against collision height at its own world X/Z.
                var t=(f.quality.w-f.upEye.w)/down;
                if t>0. && t<=d+3. {
                    let h=waterHeight(eyeRay*t);
                    if h > -1e6 {
                        t=(h-f.upEye.w)/down;
                        if t>0. && t<=d+3. && abs(waterHeight(eyeRay*t)-h)<12. {
                            origin=eyeRay*t; reflectionNormal=f.upEye.xyz; wet=.88;
                        }
                    }
                }
            }
            if wet>.015 {
                let hit=reflectedColor(origin,reflectionNormal);
                let fresnel=.10+.90*pow(1.-clamp(dot(normalize(-origin),reflectionNormal),0.,1.),4.);
                radiance=mix(radiance,hit.rgb,clamp(wet*fresnel*hit.a*f.amounts.z,0.,.85));
            }
        }
        if f.atmosphere.x>0. {
            let haze=(1.-exp(-max(d-f.fog.w,0.)*f.atmosphere.y))*min(f.atmosphere.x,.8);
            radiance=mix(radiance,linear(f.fog.rgb),haze);
        }
    }
    if f.atmosphere.z>0. && f.sun.w>.01 && f.sun.z<-.01 {
        let sunUV=project(f.sun.xyz); let edge=length(sunUV-clamp(sunUV,vec2f(0),vec2f(1)));
        if edge<.2 {
            var openings=0.;
            for(var i=1u;i<=6u;i++) {
                let testUV=mix(uv,sunUV,f32(i)*.12);
                if bounded(testUV) && distanceAt(testUV)>f.depth.w*.96 {openings+=1./6.;}
            }
            radiance+=vec3f(1.,.83,.60)*(.13*f.atmosphere.z*f.sun.w*openings)*
               exp(-3.*length(uv-sunUV))*(1.-smoothstep(0.,.2,edge))*smoothstep(60.,1200.,d);
        }
    }
    // Store a signed lighting delta; apply it to the original full-resolution scene.
    let delta=sqrt(max(radiance,vec3f(0)))-sqrt(base);
    let logDepth=clamp(log2(max(d,1.))/log2(max(f.depth.w,2.)),0.,1.);
    return vec4f(clamp(delta+vec3f(128./255.),vec3f(0),vec3f(1)),(.02+.98*logDepth));
}
@fragment fn composite(v:Vertex) -> @location(0) vec4f {
    let uv=v.uv; let base=colorAt(uv);
    let dim=vec2i(textureDimensions(illumination));
    let pos=uv*vec2f(dim)-.5; let ij=vec2i(floor(pos)); let frac=fract(pos);
    let referenceDepth=.02+.98*clamp(log2(max(distanceAt(uv),1.))/log2(max(f.depth.w,2.)),0.,1.);
    var sum=vec3f(0); var weight=0.;
    for(var y=0;y<2;y++){for(var x=0;x<2;x++){
        let s=textureLoad(illumination,clamp(ij+vec2i(x,y),vec2i(0),dim-1),0);
        let bil=mix(1.-frac,frac,vec2f(f32(x),f32(y)));
        let w=bil.x*bil.y*exp(-110.*abs(referenceDepth-s.a))*select(0.,1.,s.a>.005);
        sum+=(s.rgb-vec3f(128./255.))*w; weight+=w;
    }}
    let c=max(base+sum/max(weight,.025),vec3f(0)); let x=linear(c);
    let film=sqrt(clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),vec3f(0),vec3f(1)));
    return vec4f(mix(c,film,clamp(f.amounts.w,0.,1.)),1.);
}
