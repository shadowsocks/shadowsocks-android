add_library(predicate
    BPredicate.c
    ${PROJECT_SOURCE_DIR}/generated/flex_BPredicate.c
    ${PROJECT_SOURCE_DIR}/generated/bison_BPredicate.c
)
target_link_libraries(predicate system)
