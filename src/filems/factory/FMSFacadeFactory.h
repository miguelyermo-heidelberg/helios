#pragma once

#include <filems/facade/FMSFacade.h>
#include <sim/comps/Survey.h>

#include <memory>

namespace helios { namespace filems{

using std::shared_ptr;

/**
 * @author Alberto M. Esmoris Pena
 * @version 1.0
 * @brief Factory to build FMS facades
 */
class FMSFacadeFactory{
public:
    // ***  CONSTRUCTION / DESTRUCTION  *** //
    // ************************************ //
    /**
     * @brief Default constructor for FMS facade factory
     */
    FMSFacadeFactory() = default;
    virtual ~FMSFacadeFactory() = default;

    // ***  BUILD METHODS  *** //
    // *********************** //
    /**
     * @brief Build a FMS facade connected with given survey
     * @param outdir Root directory for output files
     * @param lasScale Specify LAS format scale factor
     * @param lasOutput Flag to specify LAS output format. True implies using
     *  LAS output format, false implies don't
     * @param las10 Flag to specify that the output format must be LAS v1.0.
     * @param zipOutput Flag to specify output zipping. True implies output
     *  will be zipped, false means it will not
     * @param survey The survey to which the facade must be connected
     * @return Built FMS facade
     */
    shared_ptr<FMSFacade> buildFacade(
        string const &outdir,
        double const lasScale,
        bool const lasOutput,
        bool const las10,
        bool const zipOutput,
        Survey &survey
    );


};

}}