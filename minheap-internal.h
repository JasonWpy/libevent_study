/*
 * Copyright (c) 2007-2012 Niels Provos and Nick Mathewson
 *
 * Copyright (c) 2006 Maxim Yegorushkin <maxim.yegorushkin@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef MINHEAP_INTERNAL_H_INCLUDED_
#define MINHEAP_INTERNAL_H_INCLUDED_

#include "event2/event-config.h"
#include "evconfig-private.h"
#include "event2/event.h"
#include "event2/event_struct.h"
#include "event2/util.h"
#include "util-internal.h"
#include "mm-internal.h"

typedef struct min_heap
{
	struct event** p;
	unsigned n, a;
} min_heap_t;

static inline void	     min_heap_ctor_(min_heap_t* s);
static inline void	     min_heap_dtor_(min_heap_t* s);
static inline void	     min_heap_elem_init_(struct event* e);
static inline int	     min_heap_elt_is_top_(const struct event *e);
static inline int	     min_heap_empty_(min_heap_t* s);
static inline unsigned	     min_heap_size_(min_heap_t* s);
static inline struct event*  min_heap_top_(min_heap_t* s);
static inline int	     min_heap_reserve_(min_heap_t* s, unsigned n);
static inline int	     min_heap_push_(min_heap_t* s, struct event* e);
static inline struct event*  min_heap_pop_(min_heap_t* s);
static inline int	     min_heap_adjust_(min_heap_t *s, struct event* e);
static inline int	     min_heap_erase_(min_heap_t* s, struct event* e);
static inline void	     min_heap_shift_up_(min_heap_t* s, unsigned hole_index, struct event* e);
static inline void	     min_heap_shift_up_unconditional_(min_heap_t* s, unsigned hole_index, struct event* e);
static inline void	     min_heap_shift_down_(min_heap_t* s, unsigned hole_index, struct event* e);

#define min_heap_elem_greater(a, b) \
	(evutil_timercmp(&(a)->ev_timeout, &(b)->ev_timeout, >))

void min_heap_ctor_(min_heap_t* s) { s->p = 0; s->n = 0; s->a = 0; }
void min_heap_dtor_(min_heap_t* s) { if (s->p) mm_free(s->p); }
void min_heap_elem_init_(struct event* e) { e->ev_timeout_pos.min_heap_idx = -1; }
int min_heap_empty_(min_heap_t* s) { return 0u == s->n; }
unsigned min_heap_size_(min_heap_t* s) { return s->n; }
struct event* min_heap_top_(min_heap_t* s) { return s->n ? *s->p : 0; }

//向最小堆中插入一个对元素
int min_heap_push_(min_heap_t* s, struct event* e)
{
	//最小堆空间是否足够，不够则进行分配
	if (min_heap_reserve_(s, s->n + 1))
		return -1;
	//将要放入的最小堆的堆元素放到，最小堆数组的最后面，然后上浮到对应的位置
	min_heap_shift_up_(s, s->n++, e);
	return 0;
}

//取出最小堆的根结点
struct event* min_heap_pop_(min_heap_t* s)
{
	//最小堆为空，则返回NILL
	if (s->n)
	{
		//取出最小堆的根结点
		struct event* e = *s->p;
		//将最小堆最后的堆元素放入到根结点的位置并进行下浮
		min_heap_shift_down_(s, 0u, s->p[--s->n]);
		e->ev_timeout_pos.min_heap_idx = -1;
		return e;
	}
	return 0;
}

//判断某个堆元素是否是跟结点
int min_heap_elt_is_top_(const struct event *e)
{
	return e->ev_timeout_pos.min_heap_idx == 0;
}

//在最小堆中销毁e，并用堆的最后一个堆元素进行替换和调整
int min_heap_erase_(min_heap_t* s, struct event* e)
{
	if (-1 != e->ev_timeout_pos.min_heap_idx)
	{
		//取出最后一个堆元素
		struct event *last = s->p[--s->n];
		unsigned parent = (e->ev_timeout_pos.min_heap_idx - 1) / 2;
		/* we replace e with the last element in the heap.  We might need to
		   shift it upward if it is less than its parent, or downward if it is
		   greater than one or both its children. Since the children are known
		   to be less than the parent, it can't need to shift both up and
		   down. */
		//如果e不是根结点，并且准备销毁的结点的父节点比最后一个堆元素大（不符合最小堆），那么
		//就进行上浮（用于无条件上浮，确保被销毁），否则就需要下浮
		if (e->ev_timeout_pos.min_heap_idx > 0 && min_heap_elem_greater(s->p[parent], last))
			min_heap_shift_up_unconditional_(s, e->ev_timeout_pos.min_heap_idx, last);
		else
			min_heap_shift_down_(s, e->ev_timeout_pos.min_heap_idx, last);
		e->ev_timeout_pos.min_heap_idx = -1;
		return 0;
	}
	return -1;
}

//调整e在最小堆的位置，e可能是堆中原本的堆元素或不是堆元素
int min_heap_adjust_(min_heap_t *s, struct event *e)
{
	if (-1 == e->ev_timeout_pos.min_heap_idx) {
		//如果e不是堆元素，则直接添加到堆中去
		return min_heap_push_(s, e);
	} else {
		//否则就看e是否符合最小堆，不符合就需要进行上浮或下浮操作
		unsigned parent = (e->ev_timeout_pos.min_heap_idx - 1) / 2;
		/* The position of e has changed; we shift it up or down
		 * as needed.  We can't need to do both. */
		if (e->ev_timeout_pos.min_heap_idx > 0 && min_heap_elem_greater(s->p[parent], e))
			min_heap_shift_up_unconditional_(s, e->ev_timeout_pos.min_heap_idx, e);
		else
			min_heap_shift_down_(s, e->ev_timeout_pos.min_heap_idx, e);
		return 0;
	}
}

//这个函数的作用是根据n决定是否重新分配最小堆数组的大小
int min_heap_reserve_(min_heap_t* s, unsigned n)
{
	//如果数组的容量还够，直接返回
	if (s->a < n)
	{
		struct event** p;
		//如果最小堆数组的大小为0，只分配数组大小为8
		//否则，则重新动态分配的大小为原先大小的2倍
		unsigned a = s->a ? s->a * 2 : 8;
		if (a < n)
			a = n;
		if (!(p = (struct event**)mm_realloc(s->p, a * sizeof *p)))
			return -1;
		s->p = p;   //重新分配之后最小堆的数组
		s->a = a;   //重新分配之后最小堆的大小
	}
	return 0;
}

//和min_heap_shift_up_类似，只是一次无条件上浮
void min_heap_shift_up_unconditional_(min_heap_t* s, unsigned hole_index, struct event* e)
{
    unsigned parent = (hole_index - 1) / 2;
    do
    {
	(s->p[hole_index] = s->p[parent])->ev_timeout_pos.min_heap_idx = hole_index;
	hole_index = parent;
	parent = (hole_index - 1) / 2;
    } while (hole_index && min_heap_elem_greater(s->p[parent], e));
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

//将某个叶子结点的堆元素上浮到合适的位置，用于堆元素的插入
void min_heap_shift_up_(min_heap_t* s, unsigned hole_index, struct event* e)
{
	//根据最小堆中堆元素的个数，计算出要插入节点e的父结点的下标
    unsigned parent = (hole_index - 1) / 2;
	//如果hole_index等于0，也就是说最小堆为空，那么就不会进入while，e就是根结点了
	//否则，e就与其父节点比较
    while (hole_index && min_heap_elem_greater(s->p[parent], e))
    {
		//如果父节点大于e，显然是不符合最小堆的定义，那么就移动父节点到新的下标上
		//仅紧接着看e是否能放到原先父节点的位置上，其实就是继续和父节点比较
	(s->p[hole_index] = s->p[parent])->ev_timeout_pos.min_heap_idx = hole_index;
	hole_index = parent;
	parent = (hole_index - 1) / 2;
    }
	//已经找到适合放置e了
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

//将某个叶子结点的堆元素下浮到合适的位置，用于堆元素的取出
void min_heap_shift_down_(min_heap_t* s, unsigned hole_index, struct event* e)
{
	//下标为hole_index的右孩子的下标
    unsigned min_child = 2 * (hole_index + 1);
	//如果对应的hole_index有右孩子，那么取出hole_index就简单了
    while (min_child <= s->n)
	{
		//根据运算符的优先级，我们可以知道表达式(min_child == s->n)先计算，
		//该表达式为真表示hole_index仅有左孩子，也就不会再计算后面表达的值。
		//否则，就以看右孩子是否大于左孩子，如果是整个表达式为真（1）
		//那么整个表达式就很清晰了：用于计算hole_index左右子结点最小的那个
	min_child -= min_child == s->n || min_heap_elem_greater(s->p[min_child], s->p[min_child - 1]);
		//把e放到hole_index下标能符合最小堆的定义，就不需要下浮了
	if (!(min_heap_elem_greater(e, s->p[min_child])))
	    break;
		//否则，把左右子结点中最小的放入hole_index下标的位置，然后继续看空出的位置是否能
		//放入e满足最小堆的定义，不行就进行 下浮。
	(s->p[hole_index] = s->p[min_child])->ev_timeout_pos.min_heap_idx = hole_index;
	hole_index = min_child;
	min_child = 2 * (hole_index + 1);
	}
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

#endif /* MINHEAP_INTERNAL_H_INCLUDED_ */
