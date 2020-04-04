#ifndef diff_hh_INCLUDED
#define diff_hh_INCLUDED

// Implementation of the linear space variant of the algorithm described in
// "An O(ND) Difference Algorithm and Its Variations"
// (http://xmailserver.org/diff2.pdf)

#include <algorithm>
#include <functional>
#include <memory>

namespace Kakoune
{

// A snake is an edit followed by a (possibly empty) diagonal
struct Snake
{
    // The end points of the diagonal (x, y) -> (u, v)
    int x, y, u, v;
    // the edit op, reverse op happen at the end of the diagonal
    enum Op { Add, Del, RevAdd, RevDel } op;
};

template<bool forward, typename IteratorA, typename IteratorB, typename Equal>
Snake find_end_snake_of_further_reaching_dpath(IteratorA a, int N, IteratorB b, int M,
                                               const int* V, const int D, const int k, Equal eq)
{
    const bool add = k == -D or (k != D and V[k-1] < V[k+1]);

    // if diagonal on the right goes further along x than diagonal on the left,
    // then we take a vertical edge from it to this diagonal, hence x = V[k+1]
    // else, we take an horizontal edge from our left diagonal,x = V[k-1]+1
    const int x = add ? V[k+1] : V[k-1]+1;
    // we are by construction on diagonal k, so our position along b (y) is x - k.
    const int y = x - k;

    auto at = [](auto&& base, int index, int size) -> decltype(auto) {
        return forward ? base[index] : base[size - 1 - index];
    };

    int u = x, v = y;
    // follow end snake along diagonal k
    while (u < N and v < M and eq(at(a, u, N), at(b, v, M)))
        ++u, ++v;

    return { x, y, u, v, add ? Snake::Add : Snake::Del };
}

template<typename IteratorA, typename IteratorB, typename Equal>
Snake find_middle_snake(IteratorA a, int N, IteratorB b, int M,
                        int* V1, int* V2, int cost_limit, Equal eq)
{
    const int delta = N - M;
    V1[1] = 0;
    V2[1] = 0;

    const int max_D = std::min((M + N + 1) / 2 + 1, cost_limit);
    for (int D = 0; D < max_D; ++D)
    {
        for (int k1 = -D; k1 <= D; k1 += 2)
        {
            auto p = find_end_snake_of_further_reaching_dpath<true>(a, N, b, M, V1, D, k1, eq);
            V1[k1] = p.u;

            const int k2 = -(k1 - delta);
            if ((delta % 2 != 0) and -(D-1) <= k2 and k2 <= (D-1) and V1[k1] + V2[k2] >= N)
                return p;// return last snake on forward path, len = (2 * D - 1)
        }

        for (int k2 = -D; k2 <= D; k2 += 2)
        {
            auto p = find_end_snake_of_further_reaching_dpath<false>(a, N, b, M, V2, D, k2, eq);
            V2[k2] = p.u;

            const int k1 = -(k2 - delta);
            if ((delta % 2 == 0) and -D <= k1 and k1 <= D and V1[k1] + V2[k2] >= N)
                return { N - p.u, M - p.v, N - p.x , M - p.y,
                         (Snake::Op)(p.op + Snake::RevAdd) };// return last snake on reverse path, len = 2 * D
        }
    }

    // We did not find a minimal path in less than max_D iterations, iterate one more time finding the best
    Snake best{};
    auto score = [](const Snake& s) { return s.u + s.v; };
    for (int k1 = -max_D; k1 <= max_D; k1 += 2)
    {
        auto p = find_end_snake_of_further_reaching_dpath<true>(a, N, b, M, V1, max_D, k1, eq);
        V1[k1] = p.u;
        if ((delta % 2 != 0) and p.u <= N and p.v <= M and score(p) >= score(best))
            best = p;
    }
    for (int k2 = -max_D; k2 <= max_D; k2 += 2)
    {
        auto p = find_end_snake_of_further_reaching_dpath<false>(a, N, b, M, V2, max_D, k2, eq);
        V2[k2] = p.u;
        if ((delta % 2 == 0) and p.u <= N and p.v <= M and score(p) >= score(best))
            best = {p.x, p.y, p.u, p.v, (Snake::Op)(p.op + Snake::RevAdd)};
    }

    if (best.op >= Snake::RevAdd) // reverse the snake now, as we were comparing snake length
        best = { N - best.u, M - best.v, N - best.x , M - best.y, best.op };
    return best;
}

enum class DiffOp
{
    Keep,
    Add,
    Remove
};

template<typename IteratorA, typename IteratorB, typename Equal, typename OnDiff>
void find_diff_rec(IteratorA a, int begA, int endA,
                   IteratorB b, int begB, int endB,
                   int* V1, int* V2, int cost_limit,
                   Equal eq, OnDiff&& on_diff)
{
    auto on_diff_ifn = [&](DiffOp op, int len) {
        if (len != 0)
            on_diff(op, len);
    };

    int prefix_len = 0;
    while (begA != endA and begB != endB and eq(a[begA], b[begB]))
         ++begA, ++begB, ++prefix_len;

    int suffix_len = 0;
    while (begA != endA and begB != endB and eq(a[endA-1], b[endB-1]))
        --endA, --endB, ++suffix_len;

    on_diff_ifn(DiffOp::Keep, prefix_len);

    const auto lenA = endA - begA, lenB = endB - begB;

    if (lenA == 0)
        on_diff_ifn(DiffOp::Add, lenB);
    else if (lenB == 0)
        on_diff_ifn(DiffOp::Remove, lenA);
    else
    {
        auto snake = find_middle_snake(a + begA, lenA, b + begB, lenB, V1, V2, cost_limit, eq);
        kak_assert(snake.u <= lenA and snake.v <= lenB);

        find_diff_rec(a, begA, begA + snake.x - (int)(snake.op == Snake::Del),
                      b, begB, begB + snake.y - (int)(snake.op == Snake::Add),
                      V1, V2, cost_limit, eq, on_diff);

        if (snake.op == Snake::Add)
            on_diff_ifn(DiffOp::Add, 1);
        if (snake.op == Snake::Del)
            on_diff_ifn(DiffOp::Remove, 1);

        on_diff_ifn(DiffOp::Keep, snake.u - snake.x);

        if (snake.op == Snake::RevAdd)
            on_diff_ifn(DiffOp::Add, 1);
        if (snake.op == Snake::RevDel)
            on_diff_ifn(DiffOp::Remove, 1);

        find_diff_rec(a, begA + snake.u + (int)(snake.op == Snake::RevDel), endA,
                      b, begB + snake.v + (int)(snake.op == Snake::RevAdd), endB,
                      V1, V2, cost_limit, eq, on_diff);
    }

    on_diff_ifn(DiffOp::Keep, suffix_len);
}

struct Diff
{
    DiffOp op;
    int len;
};

template<typename IteratorA, typename IteratorB, typename OnDiff, typename Equal = std::equal_to<>>
void for_each_diff(IteratorA a, int N, IteratorB b, int M, OnDiff&& on_diff, Equal eq = Equal{})
{
    const int max = 2 * (N + M) + 1;
    std::unique_ptr<int[]> data(new int[2*max]);
    constexpr int cost_limit = 1000;

    Diff last{};
    find_diff_rec(a, 0, N, b, 0, M, &data[N+M], &data[max + N+M], cost_limit, eq,
                  [&last, &on_diff](DiffOp op, int len) {
                      if (last.op == op)
                          last.len += len;
                      else
                      {
                          if (last.len != 0)
                              on_diff(last.op, last.len);
                          last = Diff{op, len};
                      }
                  });
    if (last.op != DiffOp{} or last.len != 0)
        on_diff(last.op, last.len);
}

}

#endif // diff_hh_INCLUDED
