#include "checkedthreads.h"
#include "time.h"
#include <algorithm>
#include <stdio.h>
#ifdef CT_TBB
#include <tbb/tbb.h>
#endif

void a(void*) { printf("a\n"); }
void b(void*) { printf("b\n"); }

#define MIN_PAR (1024*32)
#define PAR
int N = 1024*1024*16;

void merge(int numbers[], int temp[], int left, int mid, int right)
{
    int i, left_end, num_elements, tmp_pos;

    left_end = mid - 1;
    tmp_pos = left;
    num_elements = right - left + 1;

    while ((left <= left_end) && (mid <= right))
    {
        if (numbers[left] <= numbers[mid])
        {
            temp[tmp_pos] = numbers[left];
            tmp_pos = tmp_pos + 1;
            left = left +1;
        }
        else
        {
            temp[tmp_pos] = numbers[mid];
            tmp_pos = tmp_pos + 1;
            mid = mid + 1;
        }
    }

    while (left <= left_end)
    {
        temp[tmp_pos] = numbers[left];
        left = left + 1;
        tmp_pos = tmp_pos + 1;
    }
    while (mid <= right)
    {
        temp[tmp_pos] = numbers[mid];
        mid = mid + 1;
        tmp_pos = tmp_pos + 1;
    }

    for (i=0; i < num_elements; i++)
    {
        numbers[right] = temp[right];
        right = right - 1;
    }
}

void m_sort(int numbers[], int temp[], int left, int right)
{
    if(right - left < MIN_PAR) {
        std::sort(numbers+left, numbers+right+1);
        return;
    }
    int mid;

    if (right > left)
    {
        mid = (right + left) / 2;
#ifdef PAR
        ctx_invoke(
           [=] { m_sort(numbers, temp, left, mid); },
           [=] { m_sort(numbers, temp, mid+1, right); }
        );
#else
        m_sort(numbers, temp, left, mid);
        m_sort(numbers, temp, mid+1, right);
#endif

        merge(numbers, temp, left, mid+1, right);
    }
}
 
void mergeSort(int numbers[], int temp[], int array_size)
{
      m_sort(numbers, temp, 0, array_size - 1);
}
 

template<class T>
void quicksort(T* beg, T* end) {
    if (end-beg >= MIN_PAR) {
        int piv = *beg, l = 1, r = end-beg;
        while (l < r) {
            if (beg[l] <= piv) 
                l++;
            else
                std::swap(beg[l], beg[--r]);
        }
        std::swap(beg[--l], beg[0]);
#ifdef PAR
        ctx_invoke(
            [=] { quicksort(beg, beg+l); },
            [=] { quicksort(beg+r, end); }
        );
#else
        quicksort(beg, beg+l);
        quicksort(beg+r, end);
#endif
    }
    else {
        std::sort(beg, end);
    }
}


void print_and_check_results(int array[]) {
    int i;
    printf("results:");
    for(i=0; i<3; ++i) {
        printf(" %d", array[i]);
    }
    printf(" ...");
    for(i=N-3; i<N; ++i) {
        printf(" %d", array[i]);
    }
    printf("\n");
    for(i=0; i<N; ++i) {
        if(array[i] != i) {
            printf("error at %d!\n", i);
            exit(1);
        }
    }
}
int main(int argc, char** argv)
{
    if(argc>1) {
        N = atoi(argv[1]);
    }
    ct_init(0);
#if 0
    ct_task tasks[] = {
        {a,0},
        {b,0},
        {0,0}
    };
    ct_invoke(tasks);
    ctx_invoke(
        [] { printf("A\n"); },
        [] { printf("B\n"); }
    );
#endif
    int* nums=0;
    const char* descr[] = {
        "quicksort",
        "mergesort",
#ifdef CT_TBB
        "TBB  sort",
#endif
    };
    const int nsorts = sizeof descr / sizeof descr[0];

    for(int t=0; t<nsorts; ++t) {
        nums = new int[N];
        int i;
        for(i=0; i<N; ++i) {
            nums[i] = i;
        }
        std::random_shuffle(nums, nums+N);

        unsigned long long usec_start = curr_usec();
        switch(t) {
            case 0: quicksort(nums, nums+N); break;
            case 1: mergeSort(nums, new int[N], N); break;
#ifdef CT_TBB
            case 2: tbb::parallel_sort(nums, nums+N); break;
#endif
            default: break;
        };
        unsigned long long usec_finish = curr_usec();
        printf("%s: %f seconds\n", descr[t], (usec_finish - usec_start)/1000000.);

        print_and_check_results(nums);
    }
    
    ct_fini();
}
