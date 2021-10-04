#ifndef FRAMEWORK_MACROS__INCLUDED
#define FRAMEWORK_MACROS__INCLUDED

#ifndef DEFINE_RULE_OF_FIVE_DEFAULT

#define DEFINE_RULE_OF_FIVE_DEFAULT(__CLASS__)                                  \
  public:                                                                       \
   __CLASS__(const __CLASS__&) = default;                                       \
   __CLASS__(__CLASS__&&) noexcept = default;                                   \
   __CLASS__& operator=(__CLASS__ &&) noexcept = default;                       \
   __CLASS__& operator=(const __CLASS__&) = default;                            \

#endif  // DEFINE_RULE_OF_FIVE_DEFAULT

#endif // FRAMEWORK_MACROS__INCLUDED