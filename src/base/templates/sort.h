#ifndef QS_T
#   error "QS_T must be defined before including this file"
#endif

#ifndef QS_COMPARE
#   error "QS_COMPARE(a,b) must be defined (return < 0, 0, > 0)"
#endif

#ifndef QS_SWAP
#   define QS_SWAP(a,b) do { QS_T tmp = *(a); *(a) = *(b); *(b) = tmp; } while(0)
#endif

#define QS_FN(name) DOT_CONCAT(name##_, QS_T)

internal void QS_FN(quicksort)(QS_T *arr, i64 left, i64 right);

// (jd) NOTE: Haven't actually tested this
internal void
QS_FN(quicksort)(QS_T *arr, i64 left, i64 right)
{
    // Manual stack for ranges
    i64 stack_left[64];
    i64 stack_right[64];
    i32 sp = 0;

    stack_left[sp]  = left;
    stack_right[sp] = right;
    sp++;

    while(sp > 0){
        sp--;
        left  = stack_left[sp];
        right = stack_right[sp];

        while(left < right){
            i64 i = left;
            i64 j = right;
            QS_T pivot = arr[(left + right) >> 1];

            for(;;){
                while(QS_COMPARE(arr[i], pivot) < 0) i++;
                while(QS_COMPARE(arr[j], pivot) > 0) j--;

                if(i > j) break;

                QS_SWAP(&arr[i], &arr[j]);
                i++;
                j--;
            }

            // Push larger partition, iterate on smaller (same logic as tail recursion elimination)
            if((j - left) < (right - i)){
                if(i < right){
                    stack_left[sp]  = i;
                    stack_right[sp] = right;
                    sp++;
                }
                right = j;
            } else {
                if(left < j){
                    stack_left[sp]  = left;
                    stack_right[sp] = j;
                    sp++;
                }
                left = i;
            }
        }
    }
}

#undef QS_T
#undef QS_COMPARE
#undef QS_SWAP
