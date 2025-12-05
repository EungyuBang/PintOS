/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "include/threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
extern struct lock filesys_lock;
/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;


	if(file_page->file == NULL) return;

	if(pml4_get_page(thread_current()->pml4, page->va) != NULL) {
		if(pml4_is_dirty(thread_current()->pml4, page->va)) {
			void *kva = page->frame->kva;
			file_write_at(file_page->file, kva, file_page->read_bytes, file_page->ofs);
			pml4_set_dirty(thread_current()->pml4, page->va, 0);
		}
		pml4_clear_page(thread_current()->pml4, page->va);
}

	if(page->frame != NULL) {
		page->frame->page = NULL;
		page->frame = NULL;
	}
}


void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) 
{
	struct file *mmap_file = file_reopen(file);
	if(mmap_file == NULL) return NULL;

	void *start_addr = addr;

	size_t file_len = file_length(file);
	if(file_len < length) length = file_len;

	size_t read_bytes = length;
	size_t zero_bytes = pg_round_up(length) - read_bytes;

	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		
		struct lazy_load_info *aux = malloc(sizeof(struct lazy_load_info));
		if(aux == NULL) return NULL;

		aux->file = mmap_file;
		aux->ofs = offset;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;

		if(!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, aux)) {
			free(aux);
			return NULL;
		}
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
	}
	return start_addr;
}


void
do_munmap(void *addr) {
    struct supplemental_page_table *spt = &thread_current()->spt;
    struct page *page = spt_find_page(spt, addr);

    if (page == NULL) 
        return;
    
    if (page_get_type(page) != VM_FILE) 
        return;

    /* 1. 기준 파일 포인터 추출 */
    struct file *target_file = NULL;
    
    if (page->operations->type == VM_UNINIT) {
        struct lazy_load_info *info = (struct lazy_load_info *)page->uninit.aux;
        if (info) 
            target_file = info->file;
    } else {
        target_file = page->file.file;
    }

    if (target_file == NULL) 
        return;

    /* 2. 같은 파일의 모든 페이지 순회 */
    while (page != NULL) {
        /* 2-1. 타입 확인 */
        if (page_get_type(page) != VM_FILE)
            break;

        /* 2-2. 현재 페이지의 파일 확인 */
        struct file *curr_file = NULL;
        
        if (page->operations->type == VM_UNINIT) {
            struct lazy_load_info *info = (struct lazy_load_info *)page->uninit.aux;
            if (info) 
                curr_file = info->file;
        } else {
            curr_file = page->file.file;
        }

        /* 2-3. 다른 파일이면 중단 */
        if (curr_file != target_file)
            break;

        /* 2-4. 다음 주소 미리 계산 */
        void *next_addr = (uint8_t *)page->va + PGSIZE;

        /* 2-5. 페이지 제거 */
        spt_remove_page(spt, page);

        /* 2-6. 다음 페이지 검색 */
        page = spt_find_page(spt, next_addr);
    }

    /* 3. 마지막에 파일 닫기 */
    file_close(target_file);
}