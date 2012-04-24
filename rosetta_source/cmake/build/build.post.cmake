INCLUDE(../build/libraries.cmake)
INCLUDE(../build/external.cmake)

## Library definitions
#SET( LAST_LIBRARY "empty" )
FOREACH(LIBRARY ${LIBRARIES})
	INCLUDE(../build/${LIBRARY}.cmake)
	ADD_LIBRARY(${LIBRARY} ${LINK_TYPE} ${${LIBRARY}_files})
	IF( ${LAST_LIBRARY} NOT STREQUAL "empty" )
		ADD_DEPENDENCIES( ${project} ${LAST_LIBRARY} )
	ENDIF( ${LAST_LIBRARY} NOT STREQUAL "empty" )
ENDFOREACH( LIBRARY )

## Library link order
ADD_CUSTOM_TARGET( BUILD_ROSETTA_LIBS )
SET( LINK_ROSETTA_LIBS ${LINK_EXTERNAL_LIBS} )
#FOREACH(LIBRARY ${LIBRARIES})
FOREACH(LIBRARY ${LIBRARIES})
	TARGET_LINK_LIBRARIES( ${LIBRARY} ${LINK_ROSETTA_LIBS} )
	SET( LINK_ROSETTA_LIBS "${LIBRARY};${LINK_ROSETTA_LIBS}" )
	ADD_DEPENDENCIES( BUILD_ROSETTA_LIBS ${LIBRARY} )
ENDFOREACH(LIBRARY)

SET(LINK_ALL_LIBS ${LINK_ROSETTA_LIBS} ${LINK_EXTERNAL_LIBS})
