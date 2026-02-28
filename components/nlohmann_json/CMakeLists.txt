set(srcs "")
set(include_dirs "include")
set(component_requires "")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${component_requires}
)
