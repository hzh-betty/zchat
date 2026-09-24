#ifndef ZCHAT_SERVER_SRC_COMMON_FILE_STORAGE_H_
#define ZCHAT_SERVER_SRC_COMMON_FILE_STORAGE_H_

#include "common/orm_helpers.h"

namespace zchat {

// Counts existing data too; callers must not write file_store outside this
// path.
drogon::Task<VoidResult>
StoreFileCoro(std::shared_ptr<drogon::orm::DbClient> db, FileRecord file,
              bool replace_avatar = false);

} // namespace zchat

#endif
