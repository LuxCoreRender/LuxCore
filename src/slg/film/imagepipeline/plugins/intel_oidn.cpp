#include "slg/film/imagepipeline/plugins/intel_oidn.h"

namespace slg {

ImagePipelinePlugin *IntelOIDN::Copy() const {
    return new IntelOIDN();
}

void IntelOIDN::Apply(Film &film, const u_int index) {
}

}