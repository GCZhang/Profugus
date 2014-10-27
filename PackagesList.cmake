##---------------------------------------------------------------------------##
## Profugus/PackagesList.cmake
## Thomas M. Evans
## Monday December 2 21:24:6 2013
##---------------------------------------------------------------------------##

INCLUDE(TribitsListHelpers)

##---------------------------------------------------------------------------##
## PACKAGES PROVIDED
##---------------------------------------------------------------------------##

TRIBITS_REPOSITORY_DEFINE_PACKAGES(
  ParaSails ParaSails          SS
  MCLS      MCLS               SS
  Utils     packages/Utils     SS
  Matprop   packages/Matprop   SS
  SPn       packages/SPn       SS
  MC        packages/MC        SS
  )

##---------------------------------------------------------------------------##
## PLATFORM SUPPORT
##---------------------------------------------------------------------------##

TRIBITS_DISABLE_PACKAGE_ON_PLATFORMS(Profugus Windows)

##---------------------------------------------------------------------------##
## end of PackagesList.cmake
##---------------------------------------------------------------------------##