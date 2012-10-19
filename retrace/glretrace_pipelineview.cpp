
#include "glproc.hpp"
#include "glretrace.hpp"


namespace glretrace {

static Context *pipelineContext= NULL;

bool initPipelineContext( )
{
    if(currentContext != NULL)
        return true;
    
    pipelineContext= createContext(currentContext);
    return (pipelineContext != NULL);
}

bool usePipelineContext( )
{
    if(pipelineContext == NULL)
        return false;
    
    return glws::makeCurrent(currentDrawable, pipelineContext->wsContext);
}


}       // namespace glretrace


namespace retrace {

void pipelineView( trace::Call* call )
{
    if(glretrace::initPipelineContext() == false) {
        std::cerr << "error creating pipeline view context.\n";
    } else {
        std::cerr << "got a pipeline view context.\n";
    }
    
    return;
}


}       // namespace retrace

