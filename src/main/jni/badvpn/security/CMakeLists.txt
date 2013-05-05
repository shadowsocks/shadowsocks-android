add_library(security
    BSecurity.c
    BEncryption.c
    BHash.c
    BRandom.c
    OTPCalculator.c
    OTPChecker.c
    OTPGenerator.c
)
target_link_libraries(security system threadwork ${LIBCRYPTO_LIBRARIES})
