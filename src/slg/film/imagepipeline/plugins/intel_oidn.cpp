#include "slg/film/imagepipeline/plugins/intel_oidn.h"

using namespace std;
using namespace luxrays;
using namespace slg;

ImagePipelinePlugin *IntelOIDN::Copy() const {
    return new IntelOIDN();
}

IntelOIDN::IntelOIDN() {
    device = oidn::newDevice();
    device.commit();
    filter = device.newFilter("RT");
}

void IntelOIDN::Apply(Film &film, const u_int index) {
    Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

    size_t width = film.GetWidth();
    size_t height = film.GetHeight();

    u_int pixelCount = width * height;
    shared_vector<char> colorBuffer;
    shared_vector<char> outputBuffer;
    shared_vector<char> albedoBuffer;
    shared_vector<char> normalBuffer;

    colorBuffer = std::make_shared<std::vector<char>>(3*pixelCount * sizeof(float));
    outputBuffer = std::make_shared<std::vector<char>>(3*pixelCount * sizeof(float));
    albedoBuffer = std::make_shared<std::vector<char>>(3*pixelCount * sizeof(float));
    normalBuffer = std::make_shared<std::vector<char>>(3*pixelCount * sizeof(float));

    float* color;
    float* output;
    float* albedo;
    float* normal;

    color = (float*)colorBuffer->data();
    output = (float*)outputBuffer->data();
    albedo = (float*)albedoBuffer->data();
    normal = (float*)normalBuffer->data();

    SLG_LOG("[IntelOIDNPlugin] Copying Film to input buffer");
    for (u_int i = 0; i < pixelCount; ++i) {
        const u_int i3 = i * 3;
        color[i3] = pixels[i].c[0];
        color[i3 + 1] = pixels[i].c[1];
        color[i3 + 2] = pixels[i].c[2];
    }

    filter.set("hdr", true);
    filter.setImage("color", color, oidn::Format::Float3, width, height);
    if (film.HasChannel(Film::ALBEDO)) {
        filter.setImage("albedo", albedo, oidn::Format::Float3, width, height);
    } else {SLG_LOG("[IntelOIDNPlugin] Warning: ALBEDO AOV not found")}

    if (film.HasChannel(Film::AVG_SHADING_NORMAL)) {
        filter.setImage("normal", normal, oidn::Format::Float3, width, height);
    } else {SLG_LOG("[IntelOIDNPlugin] Warning: AVG_SHADING_NORMAL AOV not found")}
    
    filter.setImage("output", output, oidn::Format::Float3, width, height);
    filter.commit();
    
    SLG_LOG("[IntelOIDNPlugin] Executing filter");
    filter.execute();

    const char* errorMessage;
    if (device.getError(errorMessage) != oidn::Error::None)
         SLG_LOG("[IntelOIDNPlugin] Error: " << errorMessage);

    SLG_LOG("[IntelOIDNPlugin] Copying Film to output buffer");
    for (u_int i = 0; i < pixelCount; ++i) {
        const u_int i3 = i * 3;
        pixels[i].c[0] = output[i3];
        pixels[i].c[1] = output[i3 + 1];
        pixels[i].c[2] = output[i3 + 2];
	}


}