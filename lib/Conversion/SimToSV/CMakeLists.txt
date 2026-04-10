add_circt_conversion_library(CIRCTSimToSV
  SimToSV.cpp
  LowerPrintFormattedProcToSV.cpp

  DEPENDS
  CIRCTConversionPassIncGen

  LINK_COMPONENTS
  Core

  LINK_LIBS PUBLIC
  CIRCTEmit
  CIRCTSim
  CIRCTSeq
)
