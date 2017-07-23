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

struct Snake
{
    int x, y, u, v;
    enum Op { Add, Del, RevAdd, RevDel } op;
};

template<typename Iterator, typename Equal>
Snake find_end_snake_of_further_reaching_dpath(Iterator a, int N, Iterator b, int M,
                                               const int* V, const int D, const int k, Equal eq)
{
    const bool add = k == -D or (k != D and V[k-1] < V[k+1]);

    // if diagonal on the right goes further along x than diagonal on the left,
    // then we take a vertical edge from it to this diagonal, hence x = V[k+1]
    // else, we take an horizontal edge from our left diagonal,x = V[k-1]+1
    const int x = add ? V[k+1] : V[k-1]+1;
    // we are by construction on diagonal k, so our position along b (y) is x - k.
    const int y = x - k;

    int u = x, v = y;
    // follow end snake along diagonal k
    while (u < N and v < M and eq(a[u], b[v]))
        ++u, ++v;

    return { x, y, u, v, add ? Snake::Add : Snake::Del };
}

template<typename Iterator, typename Equal>
Snake find_middle_snake(Iterator a, int N, Iterator b, int M,
                        int* V1, int* V2, int cost_limit, Equal eq)
{
    const int delta = N - M;
    V1[1] = 0;
    V2[1] = 0;

    std::reverse_iterator<Iterator> ra{a + N}, rb{b + M};
    const int max_D = std::min((M + N + 1) / 2 + 1, cost_limit);
    for (int D = 0; D < max_D; ++D)
    {
        for (int k1 = -D; k1 <= D; k1 += 2)
        {
            auto p = find_end_snake_of_further_reaching_dpath(a, N, b, M, V1, D, k1, eq);
            V1[k1] = p.u;

            const int k2 = -(k1 - delta);
            if ((delta % 2 != 0) and -(D-1) <= k2 and k2 <= (D-1) and V1[k1] + V2[k2] >= N)
                return p;// return last snake on forward path, len = (2 * D - 1)
        }

        for (int k2 = -D; k2 <= D; k2 += 2)
        {
            auto p = find_end_snake_of_further_reaching_dpath(ra, N, rb, M, V2, D, k2, eq);
            V2[k2] = p.u;

            const int k1 = -(k2 - delta);
            if ((delta % 2 == 0) and -D <= k1 and k1 <= D and V1[k1] + V2[k2] >= N)
                return { N - p.u, M - p.v, N - p.x , M - p.y,
                         (Snake::Op)(p.op + Snake::RevAdd) };// return last snake on reverse path, len = 2 * D
        }
    }

    // We did not find a minimal path in less than max_D iterations, iterate one more time finding the best
    Snake best{};
    for (int k1 = -max_D; k1 <= max_D; k1 += 2)
    {
        auto p = find_end_snake_of_further_reaching_dpath(a, N, b, M, V1, max_D, k1, eq);
        V1[k1] = p.u;
        if ((delta % 2 != 0) and p.u + p.v >= best.u + best.v and p.u <= N and p.v <= M)
            best = p;
    }
    for (int k2 = -max_D; k2 <= max_D; k2 += 2)
    {
        auto p = find_end_snake_of_further_reaching_dpath(ra, N, rb, M, V2, max_D, k2, eq);
        V2[k2] = p.u;
        if ((delta % 2 == 0) and p.u + p.v >= best.u + best.v and p.u <= N and p.v <= M)
            best = {p.x, p.y, p.u, p.v, (Snake::Op)(p.op + Snake::RevAdd)};
    }

    if (best.op >= Snake::RevAdd)
        best = { N - best.u, M - best.v, N - best.x , M - best.y, best.op };
    return best;
}

struct Diff
{
    enum { Keep, Add, Remove } mode;
    int len;
    int posB;
};

inline void append_diff(Vector<Diff>& diffs, Diff diff)
{
    if (diff.len == 0)
        return;

    if (not diffs.empty() and diffs.back().mode == diff.mode
        and (diff.mode != Diff::Add or
             diffs.back().posB + diffs.back().len == diff.posB))
        diffs.back().len += diff.len;
    else
        diffs.push_back(diff);
}

template<typename Iterator, typename Equal>
void find_diff_rec(Iterator a, int begA, int endA,
                   Iterator b, int begB, int endB,
                   int* V1, int* V2, int cost_limit,
                   Equal eq, Vector<Diff>& diffs)
{
    int prefix_len = 0;
    while (begA != endA and begB != endB and eq(a[begA], b[begB]))
         ++begA, ++begB, ++prefix_len;

    int suffix_len = 0;
    while (begA != endA and begB != endB and eq(a[endA-1], b[endB-1]))
        --endA, --endB, ++suffix_len;

    append_diff(diffs, {Diff::Keep, prefix_len, 0});

    const auto lenA = endA - begA, lenB = endB - begB;

    if (lenA == 0)
        append_diff(diffs, {Diff::Add, lenB, begB});
    else if (lenB == 0)
        append_diff(diffs, {Diff::Remove, lenA, 0});
    else
    {
        auto snake = find_middle_snake(a + begA, lenA, b + begB, lenB, V1, V2, cost_limit, eq);
        kak_assert(snake.u <= lenA and snake.v <= lenB);

        find_diff_rec(a, begA, begA + snake.x - (int)(snake.op == Snake::Del),
                      b, begB, begB + snake.y - (int)(snake.op == Snake::Add),
                      V1, V2, cost_limit, eq, diffs);

        if (snake.op == Snake::Add)
            append_diff(diffs, {Diff::Add, 1, begB + snake.y - 1});
        if (snake.op == Snake::Del)
            append_diff(diffs, {Diff::Remove, 1, 0});

        append_diff(diffs, {Diff::Keep, snake.u - snake.x, 0});

        if (snake.op == Snake::RevAdd)
            append_diff(diffs, {Diff::Add, 1, begB + snake.v});
        if (snake.op == Snake::RevDel)
            append_diff(diffs, {Diff::Remove, 1, 0});

        find_diff_rec(a, begA + snake.u + (int)(snake.op == Snake::RevDel), endA,
                      b, begB + snake.v + (int)(snake.op == Snake::RevAdd), endB,
                      V1, V2, cost_limit, eq, diffs);
    }

    append_diff(diffs, {Diff::Keep, suffix_len, 0});
}

template<typename Iterator, typename Equal = std::equal_to<typename std::iterator_traits<Iterator>::value_type>>
Vector<Diff> find_diff(Iterator a, int N, Iterator b, int M, Equal eq = Equal{})
{
    const int max = 2 * (N + M) + 1;
    Vector<int> data(2*max);
    Vector<Diff> diffs;
    constexpr int cost_limit = 1000;
    find_diff_rec(a, 0, N, b, 0, M, &data[N+M], &data[max + N+M], cost_limit, eq, diffs);

    return diffs;
}

}

#endif // diff_hh_INCLUDED
