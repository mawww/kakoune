#ifndef diff_hh_INCLUDED
#define diff_hh_INCLUDED

// Implementation of the linear space variant of the algorithm described in
// "An O(ND) Difference Algorithm and Its Variations"
// (http://xmailserver.org/diff2.pdf)

#include "array_view.hh"
#include "vector.hh"

#include <functional>
#include <iterator>

namespace Kakoune
{

template<typename T>
struct MirroredArray : public ArrayView<T>
{
    MirroredArray(ArrayView<T> data, int size)
        : ArrayView<T>(data), size(size)
    {
        kak_assert(2 * size + 1 <= data.size());
        (*this)[1] = 0;
    }

    [[gnu::always_inline]]
    T& operator[](int n) { return ArrayView<T>::operator[](n + size); }
    [[gnu::always_inline]]
    const T& operator[](int n) const { return ArrayView<T>::operator[](n + size); }
private:
    int size;
};

struct Snake{ int x, y, u, v; bool add; };

template<typename Iterator, typename Equal>
Snake find_end_snake_of_further_reaching_dpath(Iterator a, int N, Iterator b, int M,
                                               const MirroredArray<int>& V,
                                               const int D, const int k, Equal eq)
{
    int x; // our position along a

    const bool add = k == -D or (k != D and V[k-1] < V[k+1]);

    // if diagonal on the right goes further along x than diagonal on the left,
    // then we take a vertical edge from it to this diagonal, hence x = V[k+1]
    if (add)
        x = V[k+1];
    // else, we take an horizontal edge from our left diagonal,x = V[k-1]+1
    else
        x = V[k-1]+1;

    int y = x - k; // we are by construction on diagonal k, so our position along
                   // b (y) is x - k.

    int u = x, v = y;
    // follow end snake along diagonal k
    while (u < N and v < M and eq(a[u], b[v]))
        ++u, ++v;

    return { x, y, u, v, add };
}

struct SnakeLen : Snake
{
    SnakeLen(Snake s, int d) : Snake(s), d(d) {}
    int d;
};

template<typename Iterator, typename Equal>
SnakeLen find_middle_snake(Iterator a, int N, Iterator b, int M,
                           ArrayView<int> data1, ArrayView<int> data2,
                           Equal eq)
{
    const int delta = N - M;
    MirroredArray<int> V1{data1, N + M};
    MirroredArray<int> V2{data2, N + M};

    std::reverse_iterator<Iterator> ra{a + N}, rb{b + M};

    for (int D = 0; D <= (M + N + 1) / 2; ++D)
    {
        for (int k1 = -D; k1 <= D; k1 += 2)
        {
            auto p = find_end_snake_of_further_reaching_dpath(a, N, b, M, V1, D, k1, eq);
            V1[k1] = p.u;

            const int k2 = -(k1 - delta);
            if ((delta % 2 != 0) and -(D-1) <= k2 and k2 <= (D-1))
            {
                if (V1[k1] + V2[k2] >= N)
                    return { p, 2 * D - 1 };// return last snake on forward path
            }
        }

        for (int k2 = -D; k2 <= D; k2 += 2)
        {
            auto p = find_end_snake_of_further_reaching_dpath(ra, N, rb, M, V2, D, k2, eq);
            V2[k2] = p.u;

            const int k1 = -(k2 - delta);
            if ((delta % 2 == 0) and -D <= k1 and k1 <= D)
            {
                if (V1[k1] + V2[k2] >= N)
                    return { { N - p.u, M - p.v, N - p.x , M - p.y, p.add } , 2 * D };// return last snake on reverse path
            }
        }
    }

    kak_assert(false);
    return { {}, 0 };
}

struct Diff
{
    enum { Keep, Add, Remove } mode;
    int len;
    int posB;
};

inline void append_diff(Vector<Diff>& diffs, Diff diff)
{
    if (not diffs.empty() and diffs.back().mode == diff.mode
        and (diff.mode != Diff::Add or
             diffs.back().posB + diffs.back().len == diff.posB))
        diffs.back().len += diff.len;
    else
        diffs.push_back(diff);
}

template<typename Iterator, typename Equal>
void find_diff_rec(Iterator a, int offA, int lenA,
                   Iterator b, int offB, int lenB,
                   ArrayView<int> data1, ArrayView<int> data2,
                   Equal eq, Vector<Diff>& diffs)
{
    if (lenA > 0 and lenB > 0)
    {
        auto middle_snake = find_middle_snake(a + offA, lenA, b + offB, lenB, data1, data2, eq);
        kak_assert(middle_snake.u <= lenA and middle_snake.v <= lenB);
        if (middle_snake.d > 1)
        {
            find_diff_rec(a, offA, middle_snake.x,
                          b, offB, middle_snake.y,
                          data1, data2, eq, diffs);

            if (int len = middle_snake.u - middle_snake.x)
                append_diff(diffs, {Diff::Keep, len, 0});

            find_diff_rec(a, offA + middle_snake.u, lenA - middle_snake.u,
                          b, offB + middle_snake.v, lenB - middle_snake.v,
                          data1, data2, eq, diffs);
        }
        else
        {
            if (middle_snake.d == 1)
            {
                const int diag = middle_snake.x - (middle_snake.add ? 0 : 1);
                if (diag != 0)
                    append_diff(diffs, {Diff::Keep, diag, 0});

                if (middle_snake.add)
                    append_diff(diffs, {Diff::Add, 1, offB + diag});
                else
                    append_diff(diffs, {Diff::Remove, 1, 0});
            }
            if (int len = middle_snake.u - middle_snake.x)
                append_diff(diffs, {Diff::Keep, len, 0});
        }
    }
    else if (lenB > 0)
        append_diff(diffs, {Diff::Add, lenB, offB});
    else if (lenA > 0)
        append_diff(diffs, {Diff::Remove, lenA, 0});
}

template<typename Iterator, typename Equal = std::equal_to<typename std::iterator_traits<Iterator>::value_type>>
Vector<Diff> find_diff(Iterator a, int N, Iterator b, int M, Equal eq = Equal{})
{
    const int max = 2 * (N + M) + 1;
    Vector<int> data(2*max);
    Vector<Diff> diffs;
    find_diff_rec(a, 0, N, b, 0, M,
                  {data.data(), (size_t)max}, {data.data() + max, (size_t)max},
                  eq, diffs);

    return diffs;
}

}

#endif // diff_hh_INCLUDED
