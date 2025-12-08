/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/vaddr.h"
#include "lib/kernel/bitmap.h"
#include "threads/synch.h"

/* DO NOT MODIFY BELOW LINE */
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);
// swap
static struct disk *swap_disk;
struct bitmap *swap_table;
// 페이지 하나에 필요한 섹터 수 
#define SECTORS_PER_PAGE (PGSIZE / DISK_SECTOR_SIZE)

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

static struct lock swap_lock;

/* Initialize the data for anonymous pages */
// 11주차 익명 페이지용 초기화 함수
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	
	if(swap_disk == NULL) {
		return;
	}

	// 전체 스왑 슬롯 수 계산
	size_t swap_slot_cnt = disk_size(swap_disk) / (PGSIZE / DISK_SECTOR_SIZE);
	swap_table = bitmap_create(swap_slot_cnt);

	lock_init(&swap_lock);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;

	// anon 은 바로 메모리 위에서 실행되니까 swap_slot_index = -1 로 초기화 (0도 배열시작이니까 안 됨)
	anon_page->swap_slot_index = -1;	

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

	int slot_idx = anon_page->swap_slot_index;

	// 인덱스가 없거나, 비트맵이 비어있으면 에러
	if(slot_idx == -1 || bitmap_test(swap_table, slot_idx) == false) {
		return false;
	}

	// disk -> memory 로 데이터 옮기기
	for(int i = 0 ; i < SECTORS_PER_PAGE; i++) {
		disk_read(swap_disk, slot_idx * SECTORS_PER_PAGE + i, kva + (DISK_SECTOR_SIZE * i));
	}
	// 스왑 슬롯 비워주기 -> 데이터를 메모리로 올렸으니까 스왑 디스크 공간은 비워주기
	lock_acquire(&swap_lock);
	bitmap_set(swap_table, slot_idx, false);
	lock_release(&swap_lock);

	anon_page->swap_slot_index = -1;
	
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	
	// 비트맵에서 빈 슬롯(0) 찾고, 사용 중으로 변경(1)
	lock_acquire(&swap_lock);
	size_t slot_idx = bitmap_scan_and_flip(swap_table, 0, 1, false);
	lock_release(&swap_lock);

	if(slot_idx == BITMAP_ERROR) {
		return false;
	}
	// memory -> disk 로 데이터 옮기기
	for(int i = 0; i < SECTORS_PER_PAGE; i++) {
		disk_write(swap_disk, slot_idx * SECTORS_PER_PAGE + i, page->frame->kva + (DISK_SECTOR_SIZE * i));
	}

	anon_page->swap_slot_index = (int)slot_idx;

	// page->frame = NULL;

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	if(anon_page->swap_slot_index != -1) {
		lock_acquire(&swap_lock);
		bitmap_set(swap_table, anon_page->swap_slot_index, false);
		lock_release(&swap_lock);
		anon_page->swap_slot_index = -1;
	}
}
