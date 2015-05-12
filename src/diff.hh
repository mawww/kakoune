#include "array_view.hh"
#include "vector.hh"

namespace Kakoune
{

template<typename T>
struct MirroredArray : public ArrayView<T>
{
    MirroredArray(ArrayView<T> data, int size)
        : ArrayView<T>(data), size(size)
    {
        kak_assert(2 * size + 1 <= data.size());
    }

    T& operator[](int n) { return ArrayView<T>::operator[](n + size); }
    const T& operator[](int n) const { return ArrayView<T>::operator[](n + size); }
private:
    int size;
};

struct Snake{ int x, y, u, v; bool add; }; 

template<typename Iterator>
Snake find_end_snake_of_further_reaching_dpath(Iterator a, int N, Iterator b, int M,
                                               const MirroredArray<int>& V,
                                               const int D, const int k)
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
    while (u < N and v < M and a[u] == b[v])
        ++u, ++v;

    return { x, y, u, v, add };
}

struct SnakeLen : Snake
{
    SnakeLen(Snake s, int d) : Snake(s), d(d) {}
    int d;
};

template<typename Iterator>
SnakeLen find_middle_snake(Iterator a, int N, Iterator b, int M,
                           ArrayView<int> data1, ArrayView<int> data2)
{
    const int delta = N - M;
    MirroredArray<int> V1{data1, N + M};
    MirroredArray<int> V2{data2, N + M};

    std::reverse_iterator<Iterator> ra{a + N}, rb{b + M};

    for (int D = 0; D <= (M + N + 1) / 2; ++D)
    {
        for (int k1 = -D; k1 <= D; k1 += 2)
        {
            auto p = find_end_snake_of_further_reaching_dpath(a, N, b, M, V1, D, k1);
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
            auto p = find_end_snake_of_further_reaching_dpath(ra, N, rb, M, V2, D, k2);
            V2[k2] = p.u;

            const int k1 = -(k2 - delta);
            if ((delta % 2 == 0) and -D <= k1 and k1 <= D)
            {
                if (V1[k1] + V2[k2] >= N)
                    return { { N - p.u, M - p.v, N - p.x , M - p.y } , 2 * D };// return last snake on reverse path
            }
        }
    }

    kak_assert(false);
}

template<typename Iterator>
struct Diff
{
    bool add;
    Iterator begin;
    Iterator end;
};

template<typename Iterator>
void find_diff_rec(Iterator a, size_t N, Iterator b, size_t M,
                   ArrayView<int> data1, ArrayView<int> data2,
                   Vector<Diff<Iterator>>& diffs)
{
    if (N > 0 and M > 0)
    {
        auto middle_snake = find_middle_snake(a, N, b, M, data1, data2);
        if (middle_snake.d > 1)
        {
            find_diff_rec(a, middle_snake.x, b, middle_snake.y,
                          data1, data2, diffs);

            find_diff_rec(a + middle_snake.u, N - middle_snake.u,
                          b + middle_snake.v, M - middle_snake.v,
                          data1, data2, diffs);
        }
        else if (middle_snake.d == 1)
        {
            int diag = 0;
            while (a[diag] == b[diag])
                ++diag;

            if (middle_snake.add)
                diffs.push_back({true, b + middle_snake.y, b + middle_snake.y + 1});
            else
                diffs.push_back({false, a + middle_snake.x-1, a + middle_snake.x});
        }
    }
    else if (M > 0)
        diffs.push_back({true, b, b + M});
    else if (N > 0)
        diffs.push_back({false, a, a + N});
}

template<typename Iterator>
void compact_diffs(Vector<Diff<Iterator>>& diffs)
{
    if (diffs.size() < 2)
        return;

    auto out_it = diffs.begin();
    for (auto it = out_it + 1; it != diffs.end(); ++it)
    {
        if (it->add == out_it->add and it->begin == out_it->end)
            out_it->end = it->end;
        else if (++out_it != it)
            *out_it = *it;
    }
}

template<typename Iterator>
Vector<Diff<Iterator>> find_diff(Iterator a, size_t N, Iterator b, size_t M)
{
    Vector<int> data(4 * (N+M));
    Vector<Diff<Iterator>> diffs;
    const size_t max_D_size = 2 * (N + M) + 1;
    find_diff_rec(a, N, b, M,
                  {data.data(), max_D_size},
                  {data.data() + max_D_size, max_D_size},
                  diffs);

    // compact_diffs(diffs);

    return diffs;
}

}
