/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

// CAS (Compare And Swap) 操作通常通过 `std::atomic` 类型的成员函数 `compare_exchange_weak()`
// 或 `compare_exchange_strong()` 函数来实现。
// `compare_exchange_strong()` 的基本语义是比较一个原子变量的当前值与预期值，如果相等，则将其更新为新值。
// 如果不相等，则将原子变量的当前值赋值给预期值。这个操作是原子的，保证了线程安全。
// 详细用法可参考：https://en.cppreference.com/w/cpp/atomic/atomic/compare_exchange

#include <iostream>  // std::cout
#include <thread>    // std::thread
#include <vector>    // std::vector
#include <cassert>   // assert
#include <atomic>    // std::atomic, compare_exchange_strong

// 一个简单的链表节点
struct Node
{
  int   value;
  Node *next;
};

std::atomic<Node *> list_head{nullptr};

// 向 `list_head` 中添加一个值为 `val` 的 Node 节点。
void append_node(int val)
{
  // 使用 CAS 将新节点无锁地推入链表头
  // acquire 保证读取最新值
  Node *expected = list_head.load(std::memory_order_acquire);
  Node *new_node = new Node{val, expected};
  while (!list_head.compare_exchange_strong(
      expected, new_node, std::memory_order_release, std::memory_order_acquire)) {
    // 失败时，expected 已更新为当前头指针，需要让新节点指向最新的头并重试
    new_node->next = expected;
  }
}

int main()
{
  std::vector<std::thread> threads;
  int                      thread_num = 50;
  for (int i = 0; i < thread_num; ++i)
    threads.push_back(std::thread(append_node, i));
  for (auto &th : threads)
    th.join();

  // 注意：在 `append_node` 函数是线程安全的情况下，`list_head` 中将包含 50 个 Node 节点。
  int cnt = 0;
  for (Node *it = list_head.load(std::memory_order_acquire); it != nullptr; it = it->next) {
    std::cout << ' ' << it->value;
    cnt++;
  }
  std::cout << '\n';
  assert(cnt == thread_num);
  std::cout << cnt << std::endl;

  Node *it;
  while ((it = list_head.load(std::memory_order_relaxed))) {
    list_head.store(it->next, std::memory_order_relaxed);
    delete it;
  }
  std::cout << "passed!" << std::endl;
  return 0;
}