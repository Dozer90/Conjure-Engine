#include "conjure/core/process.h"

#include "core/system/system_internal.h"

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Public methods (constructors, destructor and the platform state live in process_<platform>.cpp)

result<process_result> process::run(const process_desc &desc) {
    CONJURE_TRY_ASSIGN(process child, start(desc));
    return child.wait();
}
