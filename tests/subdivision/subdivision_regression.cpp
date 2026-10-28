#include <cmath>
#include <iostream>
#include <stdexcept>
#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/subdiv.h"
#include "slg/textures/constfloat.h"
#include "slg/textures/math/scale.h"
#include "slg/textures/texturedefs.h"

using namespace luxrays;
struct Cleanup {
    void operator()(ExtTriangleMesh *p) const { if (p) { p->Delete(); delete p; } }
};
using OwnedMesh = std::unique_ptr<ExtTriangleMesh, Cleanup>;
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
void checkNear(float value, float expected) {
    require(std::isfinite(value) && std::abs(value - expected) < 0.0002f,
        "attribute value changed");
}
OwnedMesh input(bool unused, bool attributes) {
    const unsigned n = unused ? 6 : 4;
    VertexBuffer v(n);
    v[0] = Point(0,0,0); v[1] = Point(1,0,0);
    v[2] = Point(0,1,0); v[3] = Point(0,0,1);
    if (unused) { v[4] = Point(100,100,100); v[5] = Point(-100,-100,-100); }
    TriangleBuffer t(4);
    t[0] = Triangle(0,2,1); t[1] = Triangle(0,1,3);
    t[2] = Triangle(0,3,2); t[3] = Triangle(1,2,3);
    OwnedMesh m(new ExtTriangleMesh(std::move(v),std::move(t),NormalBuffer()));
    m->ComputeNormals();
    if (attributes) {
        for (unsigned layer : {0u, 3u}) {
            auto uv = std::make_shared<UV[]>(n);
            auto col = std::make_shared<Spectrum[]>(n);
            auto alpha = std::make_shared<float[]>(n);
            auto aov = std::make_shared<float[]>(n);
            for (unsigned i=0;i<n;++i) {
                uv[i]=UV(.25f,.75f); col[i]=Spectrum(.2f,.4f,.6f);
                alpha[i]=.8f; aov[i]=.35f;
            }
            m->SetUVs(layer,uv,n); m->SetColors(layer,col,n);
            m->SetAlphas(layer,alpha,n); m->SetVertexAOV(layer,aov,n);
        }
        // AOV without alpha at the same index exercises simple-mode selection.
        auto aov = std::make_shared<float[]>(n);
        for(unsigned i=0;i<n;++i) aov[i]=.35f;
        m->SetVertexAOV(5,aov,n);
        auto triAOV = std::make_shared<float[]>(4);
        for (unsigned i=0;i<4;++i) triAOV[i]=float(i+1);
        m->SetTriAOV(2,triAOV,4);
    }
    return m;
}
void validate(const ExtTriangleMesh &m, bool attributes) {
    const unsigned n=m.GetTotalVertexCount();
    require(n>4,"subdivision did not add vertices");
    for(unsigned i=0;i<n;++i) {
        const auto &p=m.GetVertices()[i];
        require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"nonfinite position");
        require(p.x>-1&&p.x<2&&p.y>-1&&p.y<2&&p.z>-1&&p.z<2,"unused vertex affected surface");
    }
    for(unsigned i=0;i<m.GetTotalTriangleCount();++i)
        for(unsigned j=0;j<3;++j) require(m.GetTriangles()[i].v[j]<n,"invalid index");
    require(m.HasTriAOV(2)==attributes,"triangle AOV presence changed");
    if(attributes) {
        unsigned counts[4]={};
        for(unsigned i=0;i<m.GetTotalTriangleCount();++i) {
            const float value=m.GetTriAOVs(2)[i];
            require(value>=1.f && value<=4.f && value==std::floor(value),"triangle AOV value changed");
            ++counts[unsigned(value)-1];
        }
        for(unsigned count:counts) require(count==m.GetTotalTriangleCount()/4,"triangle AOV ancestry changed");
    }
    for(unsigned layer=0;layer<EXTMESH_MAX_DATA_COUNT;++layer) {
        const bool common=attributes&&(layer==0||layer==3);
        const bool aov=common||(attributes&&layer==5);
        require(m.HasUVs(layer)==common,"UV presence changed");
        require(m.HasColors(layer)==common,"color presence changed");
        require(m.HasAlphas(layer)==common,"alpha presence changed");
        require(m.HasVertexAOV(layer)==aov,"AOV presence changed");
        if(common) {
            require(m.GetAllUVs().GetLayerSize()==n,"UV size incorrect");
            require(m.GetAllColors().GetLayerSize()==n,"color size incorrect");
            require(m.GetAllAlphas().GetLayerSize()==n,"alpha size incorrect");
            for(unsigned i=0;i<n;++i) {
                checkNear(m.GetUVs(layer)[i].u,.25f); checkNear(m.GetUVs(layer)[i].v,.75f);
                checkNear(m.GetColors(layer)[i].c[0],.2f); checkNear(m.GetColors(layer)[i].c[1],.4f);
                checkNear(m.GetColors(layer)[i].c[2],.6f); checkNear(m.GetAlphas(layer)[i],.8f);
            }
        }
        if(aov) {
            for(unsigned i=0;i<n;++i) checkNear(m.GetVertexAOVs(layer)[i],.35f);
        }
    }
}
void textureReferences() {
    slg::TextureDefinitions definitions;
    auto original = std::make_unique<slg::ConstFloatTexture>(2.f);
    original->SetName("source");
    auto [source, unused] = definitions.DefineTexture(std::move(original));
    auto scale = std::make_unique<slg::ScaleTexture>(source, source);
    scale->SetName("scale");
    definitions.DefineTexture(std::move(scale));
    auto replacement = std::make_unique<slg::ConstFloatTexture>(3.f);
    replacement->SetName("source");
    auto [updated, retired] = definitions.DefineTexture(std::move(replacement));
    // Keep retired alive while checking identity, avoiding reliance on allocator behavior.
    auto &result = static_cast<slg::ScaleTexture&>(definitions.GetTexture("scale"));
    require(&result.GetTexture1()==&updated && &result.GetTexture2()==&updated,
        "texture replacement left stale references");
    checkNear(result.Y(),9.f);
    slg::TexturePtr pointer(retired.get());
    slg::updtex(pointer, *retired, updated);
    require(pointer.get()==&updated,"pointer texture reference was not updated");
    slg::TextureConstPtr constPointer(retired.get());
    slg::updtex(constPointer,*retired,updated);
    require(constPointer.get()==&updated,"const texture pointer was not updated");
    std::reference_wrapper<const slg::Texture> constReference(*retired);
    slg::updtex(constReference,*retired,updated);
    require(&constReference.get()==&updated,"const texture reference was not updated");
    std::cout<<"PASS texture references"<<std::endl;
}
void compactionEquivalence() {
    auto compact=input(false,true), sparse=input(true,true);
    for(auto *m:{compact.get(),sparse.get()}) {
        for(unsigned i=0;i<m->GetTotalVertexCount();++i) {
            m->GetUVs(0)[i]=UV(.1f*i,.2f*i);
            m->GetColors(3)[i]=Spectrum(.1f*i,.2f*i,.3f*i);
            m->GetAlphas(3)[i]=.1f*i;
            m->GetVertexAOVs(5)[i]=.2f*i;
        }
    }
    OwnedMesh a(slg::SubdivShape::ApplySubdiv(*compact,2,true,false,.521f,10.f).release());
    OwnedMesh b(slg::SubdivShape::ApplySubdiv(*sparse,2,true,false,.521f,10.f).release());
    require(a->GetTotalVertexCount()==b->GetTotalVertexCount(),"compaction vertex count changed");
    require(a->GetTotalTriangleCount()==b->GetTotalTriangleCount(),"compaction triangle count changed");
    for(unsigned i=0;i<a->GetTotalVertexCount();++i) {
        checkNear(a->GetVertices()[i].x,b->GetVertices()[i].x);
        checkNear(a->GetVertices()[i].y,b->GetVertices()[i].y);
        checkNear(a->GetVertices()[i].z,b->GetVertices()[i].z);
        checkNear(a->GetUVs(0)[i].u,b->GetUVs(0)[i].u);
        checkNear(a->GetUVs(0)[i].v,b->GetUVs(0)[i].v);
        for(unsigned c=0;c<3;++c) checkNear(a->GetColors(3)[i].c[c],b->GetColors(3)[i].c[c]);
        checkNear(a->GetAlphas(3)[i],b->GetAlphas(3)[i]);
        checkNear(a->GetVertexAOVs(5)[i],b->GetVertexAOVs(5)[i]);
    }
    for(unsigned i=0;i<a->GetTotalTriangleCount();++i)
        for(unsigned c=0;c<3;++c) require(a->GetTriangles()[i].v[c]==b->GetTriangles()[i].v[c],"compaction topology changed");
    std::cout<<"PASS varying attributes with unused vertices"<<std::endl;
}
void creaseIndexInvariance() {
    auto make = [](bool reverse) {
        VertexBuffer v(4);
        const Point positions[] = {Point(0,0,0),Point(1,0,0),Point(0,1,0),Point(1,1,.1f)};
        auto remap=[reverse](unsigned i){ return reverse ? 3u-i : i; };
        for(unsigned i=0;i<4;++i) v[remap(i)]=positions[i];
        TriangleBuffer t(2);
        t[0]=Triangle(remap(0),remap(1),remap(2));
        t[1]=Triangle(remap(1),remap(3),remap(2));
        return OwnedMesh(new ExtTriangleMesh(std::move(v),std::move(t),NormalBuffer()));
    };
    auto a=make(false), b=make(true);
    OwnedMesh sa(slg::SubdivShape::ApplySubdiv(*a,3,true,false,.521f,10.f).release());
    OwnedMesh sb(slg::SubdivShape::ApplySubdiv(*b,3,true,false,.521f,10.f).release());
    OwnedMesh highThreshold(slg::SubdivShape::ApplySubdiv(*a,3,true,false,1.2f,10.f).release());
    require(sa->GetTotalVertexCount()==sb->GetTotalVertexCount(),"renumbering changed count");
    for(unsigned i=0;i<sa->GetTotalVertexCount();++i) {
        // Both thresholds classify the shallow interior fold as smooth while
        // retaining the same boundary creases. A binary sharpness flag must
        // never be compared with an angle, particularly one exceeding 1 radian.
        auto thresholdDelta=sa->GetVertices()[i]-highThreshold->GetVertices()[i];
        require(thresholdDelta.LengthSquared()<1e-10f,"angle threshold disabled boundary crease flags");
        float best=1e30f;
        for(unsigned j=0;j<sb->GetTotalVertexCount();++j) {
            auto d=sa->GetVertices()[i]-sb->GetVertices()[j];
            best=std::min(best,d.LengthSquared());
        }
        require(best<1e-10f,"vertex numbering changed enhanced crease geometry");
    }
    std::cout<<"PASS crease geometry independent of vertex numbering"<<std::endl;
}
int main(int argc, char **argv) {
    try {
        if(argc>1 && std::string(argv[1])=="textures") { textureReferences(); return 0; }
        if(argc>1 && std::string(argv[1])=="creases") { creaseIndexInvariance(); return 0; }
        for(bool enhanced : {true,false}) for(bool attrs : {false,true}) {
            auto src=input(enhanced,attrs);
            for(int pass=0;pass<2;++pass) {
                OwnedMesh result(slg::SubdivShape::ApplySubdiv(*src,1,enhanced,false,.521f,10.f).release());
                validate(*result,attrs);
                OwnedMesh copy(result->Copy().release()); validate(*copy,attrs);
                src=std::move(result);
            }
            std::cout<<"PASS enhanced="<<enhanced<<" attributes="<<attrs<<std::endl;
        }
        compactionEquivalence();
        creaseIndexInvariance();
        return 0;
    } catch(const std::exception &e) { std::cerr<<"FAIL: "<<e.what()<<std::endl; return 1; }
}

