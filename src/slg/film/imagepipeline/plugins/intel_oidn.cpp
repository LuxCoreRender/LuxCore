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

    vector<float> color(3*pixelCount); // A float for each color channel
    vector<float> output(3*pixelCount); 

    SLG_LOG("[IntelOIDNPlugin] Copying Film to input buffer");
    for (u_int i = 0; i < pixelCount; i += 3) {
        color[i] = pixels[i].c[0];
        color[i + 1] = pixels[i].c[1];
        color[i + 2] = pixels[i].c[2];
	}

    // TODO: HDR support
    filter.setImage("color", color.data(), oidn::Format::Float3, width, height);
    filter.setImage("output", output.data(), oidn::Format::Float3, width, height);
    filter.set("hdr", true);
    filter.commit();

    SLG_LOG("[IntelOIDNPlugin] Executing filter");
    filter.execute();
    SLG_LOG("[IntelOIDNPlugin] Copying Film to output buffer");

    const char* errorMessage;
    if (device.getError(errorMessage) != oidn::Error::None)
         SLG_LOG("[IntelOIDNPlugin] Error:" << errorMessage);

    for (u_int i = 0; i < pixelCount; i += 3) {
        pixels[i].c[0] = output[i];
        pixels[i].c[1] = output[i + 1];
        pixels[i].c[2] = output[i + 2];
	}


}