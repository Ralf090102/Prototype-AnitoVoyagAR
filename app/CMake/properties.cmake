# Set Target Properties
function(set_anito_properties)
    set_target_properties(vulkan_wrapper PROPERTIES
            VERSION ${ANITO_VERSION}
            SOVERSION ${ANITO_VERSION_MAJOR}
    )

    set_target_properties(openxr_wrapper PROPERTIES
            VERSION ${ANITO_VERSION}
            SOVERSION ${ANITO_VERSION_MAJOR}
    )
endfunction()