#include <util/config.h>

#include "LocalFileLoader.h"
#include "FileserverFileLoader.h"
#include "S3FileLoader.h"
#include "SharedFilesManager.h"

namespace storage {
    FileLoader &getFileLoader() {
        util::SystemConfig &conf = util::getSystemConfig();

        if (conf.functionStorage == "local") {
            static thread_local LocalFileLoader fl;
            return fl;
        } else if (conf.functionStorage == "s3") {
            static thread_local S3FileLoader fl;
            return fl;
        } else if (conf.functionStorage == "fileserver") {
            static thread_local FileserverFileLoader fl;
            if(fl.getFileserverUrl().empty()) {
                throw std::runtime_error("No fileserver URL set in fileserver mode");
            }
            return fl;
        } else {
            throw std::runtime_error("Invalid function storage mode");
        }
    }

    SharedFilesManager &getSharedFilesManager() {
        // Note - not thread-local but internally thread-safe
        static SharedFilesManager m;
        return m;
    }
}
