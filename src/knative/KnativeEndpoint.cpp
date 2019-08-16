#include "KnativeHandler.h"
#include "KnativeEndpoint.h"

namespace knative {
    KnativeEndpoint::KnativeEndpoint() : HttpEndpoint(8080, 8) {
    }

    void KnativeEndpoint::setHandler() {
        httpEndpoint->setHandler(Http::make_handler<KnativeHandler>());
    }
}
