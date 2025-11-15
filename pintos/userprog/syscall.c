#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
// 10주차 system call handler 만들어야 하는 곳
void
syscall_handler (struct intr_frame *f) {
	/* ======================================================================
	 * ==================== SYSTEM CALL IMPLEMENTATION ROADMAP ====================
	 * ======================================================================
	 *
	 * Pintos 프로젝트 2의 시스템 콜 핸들러 구현을 위한 로드맵입니다.
	 * 각 단계에 맞춰 아래 TODO 주석을 실제 코드로 채워나가시면 됩니다.
	 *
	 * 시스템 콜 호출 규약 (x86-64):
	 * - 시스템 콜 번호: %rax
	 * - 인자 1: %rdi
	 * - 인자 2: %rsi
	 * - 인자 3: %rdx
	 * - 인자 4: %r10
	 * - 인자 5: %r8
	 * - 인자 6: %r9
	 * - 반환값: %rax
	 */

	// 1. 시스템 콜 번호를 %rax 레지스터에서 가져옵니다.
	uint64_t syscall_no = f->R.rax;

	// 2. 가져온 시스템 콜 번호에 따라 switch 문으로 분기합니다.
	switch (syscall_no) {
		/* === 프로젝트 2: 프로세스 관련 시스템 콜 === */

		case SYS_HALT:	/* void halt (void) */
			// Pintos를 종료시킵니다.
			// threads/init.h 에 있는 power_off() 함수를 호출하면 됩니다.
			// TODO: power_off() 호출
			break;

		case SYS_EXIT:	/* void exit (int status) */
			// 현재 사용자 프로그램을 종료시킵니다.
			// 1. 종료 상태(status)를 %rdi 레지스터에서 가져옵니다.
			// 2. 현재 스레드의 종료 상태를 저장합니다. (예: thread_current()->exit_status = status;)
			// 3. thread_exit()를 호출하여 스레드를 종료시킵니다.
			// TODO: exit 구현
			break;

		case SYS_FORK:	/* pid_t fork (const char *thread_name) */
			// 현재 프로세스를 복제합니다.
			// 1. 스레드 이름(%rdi)과 부모의 intr_frame(f)을 인자로 process_fork()를 호출합니다.
			// 2. process_fork()의 반환값(자식 프로세스의 pid)을 %rax에 저장하여 리턴합니다.
			// TODO: fork 구현
			break;

		case SYS_EXEC:	/* int exec (const char *cmd_line) */
			// 현재 프로세스를 새 프로그램으로 교체합니다.
			// 1. 실행할 프로그램의 이름과 인자가 담긴 문자열 주소(%rdi)를 가져옵니다.
			// 2. 해당 주소가 유효한 사용자 영역 주소인지 확인해야 합니다. (주소 검증)
			// 3. process_exec()를 호출하여 프로세스를 교체합니다.
			// 4. process_exec()는 성공 시 리턴하지 않으므로, 실패한 경우에만 -1을 리턴합니다.
			// TODO: exec 구현
			break;

		case SYS_WAIT:	/* int wait (pid_t pid) */
			// 자식 프로세스가 종료될 때까지 기다립니다.
			// 1. 기다릴 자식 프로세스의 pid를 %rdi에서 가져옵니다.
			// 2. process_wait()를 호출하여 해당 pid의 프로세스가 종료되기를 기다립니다.
			// 3. process_wait()가 리턴하면, 자식의 종료 상태를 %rax에 저장하여 리턴합니다.
			// TODO: wait 구현
			break;

		/* === 프로젝트 2: 파일 시스템 관련 시스템 콜 === */
		/* 파일 관련 시스템 콜을 구현하기 전에 먼저 해야 할 일:
		 * 1. 스레드 구조체에 파일 디스크립터 테이블(FDT)을 추가해야 합니다.
		 *    (예: struct file **fdt;)
		 * 2. 사용자 주소의 유효성을 검사하는 함수(check_address)를 만들어야 합니다.
		 *    모든 포인터 타입의 인자는 이 함수로 검증 후 사용해야 안전합니다.
		 */

		case SYS_CREATE:/* bool create (const char *file, unsigned initial_size) */
			// 새 파일을 생성합니다.
			// 1. 파일 이름(%rdi), 초기 크기(%rsi)를 인자로 받습니다.
			// 2. 파일 이름 포인터의 유효성을 검사합니다.
			// 3. filesys/filesys.h 의 filesys_create() 함수를 호출합니다.
			// 4. 성공 여부(bool)를 %rax에 저장하여 리턴합니다.
			// TODO: create 구현
			break;

		case SYS_REMOVE:/* bool remove (const char *file) */
			// 파일을 삭제합니다.
			// 1. 파일 이름(%rdi)을 인자로 받습니다.
			// 2. 파일 이름 포인터의 유효성을 검사합니다.
			// 3. filesys/filesys.h 의 filesys_remove() 함수를 호출합니다.
			// 4. 성공 여부(bool)를 %rax에 저장하여 리턴합니다.
			// TODO: remove 구현
			break;

		case SYS_OPEN:	/* int open (const char *file) */
			// 파일을 엽니다.
			// 1. 파일 이름(%rdi)을 인자로 받습니다.
			// 2. 파일 이름 포인터의 유효성을 검사합니다.
			// 3. filesys/filesys.h 의 filesys_open()으로 파일을 엽니다.
			// 4. 파일 열기에 성공하면, 현재 프로세스의 FDT에 파일 객체 포인터를 추가하고,
			//    해당 테이블의 인덱스(fd)를 %rax에 저장하여 리턴합니다.
			// 5. 실패 시 -1을 리턴합니다.
			// TODO: open 구현
			break;

		case SYS_FILESIZE:/* int filesize (int fd) */
			// 파일의 크기를 리턴합니다.
			// 1. 파일 디스크립터(fd)를 %rdi에서 가져옵니다.
			// 2. fd를 이용해 FDT에서 파일 객체를 찾습니다.
			// 3. filesys/file.h 의 file_length() 함수로 파일 크기를 얻어옵니다.
			// 4. 파일 크기를 %rax에 저장하여 리턴합니다.
			// TODO: filesize 구현
			break;

		case SYS_READ:	/* int read (int fd, void *buffer, unsigned size) */
			// 열린 파일에서 데이터를 읽습니다.
			// 1. fd(%rdi), 버퍼 주소(%rsi), 읽을 크기(%rdx)를 인자로 받습니다.
			// 2. 버퍼 주소의 유효성을 검사합니다.
			// 3. fd가 0(STDIN)이면, input_getc()를 이용해 키보드 입력을 읽어 버퍼에 저장합니다.
			// 4. fd가 0이 아니면, FDT에서 파일 객체를 찾아 file_read()로 파일 내용을 읽습니다.
			// 5. 실제로 읽은 바이트 수를 %rax에 저장하여 리턴합니다.
			// TODO: read 구현
			break;

		case SYS_WRITE: /* int write (int fd, const void *buffer, unsigned size) */
			// 열린 파일에 데이터를 씁니다.
			// 1. fd(%rdi), 버퍼 주소(%rsi), 쓸 크기(%rdx)를 인자로 받습니다.
			// 2. 버퍼 주소의 유효성을 검사합니다.
			// 3. fd가 1(STDOUT)이면, putbuf()를 이용해 버퍼의 내용을 화면에 출력합니다.
			// 4. fd가 1이 아니면, FDT에서 파일 객체를 찾아 file_write()로 파일에 씁니다.
			// 5. 실제로 쓴 바이트 수를 %rax에 저장하여 리턴합니다.
			// TODO: write 구현
			break;

		case SYS_SEEK:	/* void seek (int fd, unsigned position) */
			// 열린 파일의 다음 읽거나 쓸 위치를 변경합니다.
			// 1. fd(%rdi), 이동할 위치(%rsi)를 인자로 받습니다.
			// 2. FDT에서 파일 객체를 찾아 file_seek()을 호출합니다.
			// TODO: seek 구현
			break;

		case SYS_TELL:	/* unsigned tell (int fd) */
			// 열린 파일의 다음 읽거나 쓸 위치를 알려줍니다.
			// 1. fd(%rdi)를 인자로 받습니다.
			// 2. FDT에서 파일 객체를 찾아 file_tell()을 호출합니다.
			// 3. 파일의 현재 위치를 %rax에 저장하여 리턴합니다.
			// TODO: tell 구현
			break;

		case SYS_CLOSE: /* void close (int fd) */
			// 열린 파일을 닫습니다.
			// 1. fd(%rdi)를 인자로 받습니다.
			// 2. FDT에서 해당 fd의 항목을 정리하고, file_close()를 호출하여 파일을 닫습니다.
			// TODO: close 구현
			break;

		default:
			// 처리하지 않은 다른 시스템 콜이 들어온 경우
			// 현재 프로세스를 종료시키는 것이 안전합니다.
			printf ("system call!\n");
			thread_exit ();
			break;
	}
}
