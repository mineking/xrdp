/*
 * debug.h
 *
 *  Created on: Aug 8, 2015
 *      Author: maillard
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#include <sys/syscall.h>



#define log_info(M, ...) g_writeln("%d:%ld [INFO] %s:%d %s() : " M "",getpid(),syscall(SYS_gettid), __FILE__, __LINE__,__func__, ##__VA_ARGS__)
#define log_error(M, ...) g_writeln("%d:%ld [ERROR] %s:%d %s() : " M "",getpid(),syscall(SYS_gettid), __FILE__, __LINE__,__func__, ##__VA_ARGS__)
#define log_order(M, ...) g_writeln("%d:%ld [ORDER] %s:%d %s() : " M "",getpid(),syscall(SYS_gettid), __FILE__, __LINE__,__func__, ##__VA_ARGS__)


#endif /* DEBUG_H_ */
