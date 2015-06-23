#ifndef __UTIL_H_FFS
#define __UTIL_H_FFS

#define ASSERT_NOT(v, nv) do{if((v) == (nv))goto faild;}while(0)
#define ASSERT_EOF(v) ASSERT_NOT(v, EOF)
#define ASSERT_EOC(v) ASSERT_NOT(v, EOC)
#define ASSERT_NULL(v) ASSERT_NOT(v, NULL)
#define ASSERT_FALSE(v) ASSERT_NOT(v, false)
#define ASSERT_SIZE(v, size) ASSERT_FALSE((v) == (size))

#define DELETE(a) do{if(a != NULL){delete a; a = NULL;}}while(0)

#endif
