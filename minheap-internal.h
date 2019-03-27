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

//����С���в���һ����Ԫ��
int min_heap_push_(min_heap_t* s, struct event* e)
{
	//��С�ѿռ��Ƿ��㹻����������з���
	if (min_heap_reserve_(s, s->n + 1))
		return -1;
	//��Ҫ�������С�ѵĶ�Ԫ�طŵ�����С�����������棬Ȼ���ϸ�����Ӧ��λ��
	min_heap_shift_up_(s, s->n++, e);
	return 0;
}

//ȡ����С�ѵĸ����
struct event* min_heap_pop_(min_heap_t* s)
{
	//��С��Ϊ�գ��򷵻�NILL
	if (s->n)
	{
		//ȡ����С�ѵĸ����
		struct event* e = *s->p;
		//����С�����Ķ�Ԫ�ط��뵽������λ�ò������¸�
		min_heap_shift_down_(s, 0u, s->p[--s->n]);
		e->ev_timeout_pos.min_heap_idx = -1;
		return e;
	}
	return 0;
}

//�ж�ĳ����Ԫ���Ƿ��Ǹ����
int min_heap_elt_is_top_(const struct event *e)
{
	return e->ev_timeout_pos.min_heap_idx == 0;
}

//����С��������e�����öѵ����һ����Ԫ�ؽ����滻�͵���
int min_heap_erase_(min_heap_t* s, struct event* e)
{
	if (-1 != e->ev_timeout_pos.min_heap_idx)
	{
		//ȡ�����һ����Ԫ��
		struct event *last = s->p[--s->n];
		unsigned parent = (e->ev_timeout_pos.min_heap_idx - 1) / 2;
		/* we replace e with the last element in the heap.  We might need to
		   shift it upward if it is less than its parent, or downward if it is
		   greater than one or both its children. Since the children are known
		   to be less than the parent, it can't need to shift both up and
		   down. */
		//���e���Ǹ���㣬����׼�����ٵĽ��ĸ��ڵ�����һ����Ԫ�ش󣨲�������С�ѣ�����ô
		//�ͽ����ϸ��������������ϸ���ȷ�������٣����������Ҫ�¸�
		if (e->ev_timeout_pos.min_heap_idx > 0 && min_heap_elem_greater(s->p[parent], last))
			min_heap_shift_up_unconditional_(s, e->ev_timeout_pos.min_heap_idx, last);
		else
			min_heap_shift_down_(s, e->ev_timeout_pos.min_heap_idx, last);
		e->ev_timeout_pos.min_heap_idx = -1;
		return 0;
	}
	return -1;
}

//����e����С�ѵ�λ�ã�e�����Ƕ���ԭ���Ķ�Ԫ�ػ��Ƕ�Ԫ��
int min_heap_adjust_(min_heap_t *s, struct event *e)
{
	if (-1 == e->ev_timeout_pos.min_heap_idx) {
		//���e���Ƕ�Ԫ�أ���ֱ����ӵ�����ȥ
		return min_heap_push_(s, e);
	} else {
		//����Ϳ�e�Ƿ������С�ѣ������Ͼ���Ҫ�����ϸ����¸�����
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

//��������������Ǹ���n�����Ƿ����·�����С������Ĵ�С
int min_heap_reserve_(min_heap_t* s, unsigned n)
{
	//������������������ֱ�ӷ���
	if (s->a < n)
	{
		struct event** p;
		//�����С������Ĵ�СΪ0��ֻ���������СΪ8
		//���������¶�̬����Ĵ�СΪԭ�ȴ�С��2��
		unsigned a = s->a ? s->a * 2 : 8;
		if (a < n)
			a = n;
		if (!(p = (struct event**)mm_realloc(s->p, a * sizeof *p)))
			return -1;
		s->p = p;   //���·���֮����С�ѵ�����
		s->a = a;   //���·���֮����С�ѵĴ�С
	}
	return 0;
}

//��min_heap_shift_up_���ƣ�ֻ��һ���������ϸ�
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

//��ĳ��Ҷ�ӽ��Ķ�Ԫ���ϸ������ʵ�λ�ã����ڶ�Ԫ�صĲ���
void min_heap_shift_up_(min_heap_t* s, unsigned hole_index, struct event* e)
{
	//������С���ж�Ԫ�صĸ����������Ҫ����ڵ�e�ĸ������±�
    unsigned parent = (hole_index - 1) / 2;
	//���hole_index����0��Ҳ����˵��С��Ϊ�գ���ô�Ͳ������while��e���Ǹ������
	//����e�����丸�ڵ�Ƚ�
    while (hole_index && min_heap_elem_greater(s->p[parent], e))
    {
		//������ڵ����e����Ȼ�ǲ�������С�ѵĶ��壬��ô���ƶ����ڵ㵽�µ��±���
		//�������ſ�e�Ƿ��ܷŵ�ԭ�ȸ��ڵ��λ���ϣ���ʵ���Ǽ����͸��ڵ�Ƚ�
	(s->p[hole_index] = s->p[parent])->ev_timeout_pos.min_heap_idx = hole_index;
	hole_index = parent;
	parent = (hole_index - 1) / 2;
    }
	//�Ѿ��ҵ��ʺϷ���e��
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

//��ĳ��Ҷ�ӽ��Ķ�Ԫ���¸������ʵ�λ�ã����ڶ�Ԫ�ص�ȡ��
void min_heap_shift_down_(min_heap_t* s, unsigned hole_index, struct event* e)
{
	//�±�Ϊhole_index���Һ��ӵ��±�
    unsigned min_child = 2 * (hole_index + 1);
	//�����Ӧ��hole_index���Һ��ӣ���ôȡ��hole_index�ͼ���
    while (min_child <= s->n)
	{
		//��������������ȼ������ǿ���֪�����ʽ(min_child == s->n)�ȼ��㣬
		//�ñ��ʽΪ���ʾhole_index�������ӣ�Ҳ�Ͳ����ټ���������ֵ��
		//���򣬾��Կ��Һ����Ƿ�������ӣ�������������ʽΪ�棨1��
		//��ô�������ʽ�ͺ������ˣ����ڼ���hole_index�����ӽ����С���Ǹ�
	min_child -= min_child == s->n || min_heap_elem_greater(s->p[min_child], s->p[min_child - 1]);
		//��e�ŵ�hole_index�±��ܷ�����С�ѵĶ��壬�Ͳ���Ҫ�¸���
	if (!(min_heap_elem_greater(e, s->p[min_child])))
	    break;
		//���򣬰������ӽ������С�ķ���hole_index�±��λ�ã�Ȼ��������ճ���λ���Ƿ���
		//����e������С�ѵĶ��壬���оͽ��� �¸���
	(s->p[hole_index] = s->p[min_child])->ev_timeout_pos.min_heap_idx = hole_index;
	hole_index = min_child;
	min_child = 2 * (hole_index + 1);
	}
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

#endif /* MINHEAP_INTERNAL_H_INCLUDED_ */
