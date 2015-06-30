#define SAVL_PARAM_NAME ChunksTree
#define SAVL_PARAM_FEATURE_COUNTS 0
#define SAVL_PARAM_FEATURE_NOKEYS 0
#define SAVL_PARAM_TYPE_ENTRY struct chunk
#define SAVL_PARAM_TYPE_KEY size_t
#define SAVL_PARAM_TYPE_ARG int
#define SAVL_PARAM_FUN_COMPARE_ENTRIES(arg, entry1, entry2) B_COMPARE((entry1)->offset, (entry2)->offset)
#define SAVL_PARAM_FUN_COMPARE_KEY_ENTRY(arg, key1, entry2) B_COMPARE((key1), (entry2)->offset)
#define SAVL_PARAM_MEMBER_NODE chunks_tree_node
