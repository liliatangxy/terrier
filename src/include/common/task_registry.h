#pragma once

#include <vector>
#include "tbb/task.h"
#include "tbb/task_arena.h"

namespace terrier::common {

/**
 * @brief Singleton class responsible for creating and maintaining the task scheduler and creating and scheduling tasks
 *
 * Task owners are able to queue tasks by calling AddTask. Once a task has begun, it cannot be preempted.
 *
 */

class TaskRegistry {
 public:
  /**
   * Creates the task arena architecture
   * @param metrics_manager pointer to the metrics manager if metrics are enabled. Necessary for worker threads to
   * register themselves
   */
  explicit TaskRegistry(common::ManagedPointer<metrics::MetricsManager> metrics_manager) {
    tbb::task_arena task_arena_;
    task_arena_.initialize();
    task_arena_list_.push_back(task_arena_);
    metrics_manager_(metrics_manager);
  }

  /**
   * Add a task to the registry.
   * We choose the arena/queue to add the task to and set the priority of the task based on the type of task being done
   * @return output of attempt to execute it
   */
  template <class F>
  auto AddTask(F&& f) {
    // TODO(Lilia): need to determine way to decide priorities and route them to task arenas or groups based
    // on originator of task
    // for now just fixed priority and single task arena
    // with fine grained tasking and TBB task management
    // not much of a need to care about which arena owns what

    return task_arena_list_[0].execute(f);
  }

  // only useful if you want to change their priorities and cancel easily
  /**
   * Adds tasks to the registry as a group.
   * Good for if you'd like to create many tasks at once, especially if they have similar lifetime
   * and priority expectations
   * We choose the arena/queue to add the task to and set the priority of the task based on the type of task being done
   * @return output of attempt to execute it
   */
  template <class F>
  auto AddTasks(std::vector<F&&> fs) {
    tbb::task_group *task_group_ = new tbb::task_group;
    for (auto *func : fs) {
      task_arena_list_[0]->execute([&, work{std::move(func)}] {
        task_group_table_[task_arena_list_[0]].push_back(task_group_);
        task_group_->run(work);
      });
    }
  }

  bool CancelTask(F&& f) {

  }

  bool CancelTasks() {

  }

  /**
   * Cleans up all task arenas after waiting for
   */
  void TearDown() {
    common::SpinLatch::ScopedSpinLatch guard(table_latch_)
    for (auto *arena : task_arena_list_) {
      // is there a reason to not fully remove the task_arena
      //entry->terminate();
      for (auto *gp : task_group_table_) {
        gp->wait();
      }
      arena->~task_arena();
      delete arena;
    }
    task_arena_list_.clear();
    task_group_table_.clear();
  }


 private:
  common::SpinLatch table_latch_;

  std::vector<tbb::task_arena *> task_arena_list_;

  // all active task_groups for a task_arena
  std::unordered_map<tbb::task_arena *, std::vector<tbb::task_group *>> task_group_table_;
};


}