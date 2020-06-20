cet_make(
  LIBRARIES
    cetlib_except
  )

install_headers(SUBDIRS details)
install_fhicl(SUBDIRS details)
install_source(SUBDIRS details)
