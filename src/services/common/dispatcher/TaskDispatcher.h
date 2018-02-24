
#pragma once


#include <consts/Enums.h>
#include <task/task/TaskHeader.h>
#include "ExecutorDispatcher.h"

namespace services {
    class TaskDispatcher {
    public:
        enum RegisterResult {
            Success,
            TypeAlreadyHasExecutor,
            DispatcherIsImmutable,
        };

        RegisterResult Register(TaskType type, std::unique_ptr<ExecutorDispatcher> executor) {
            std::unique_lock<std::mutex> mlock(mutableMutex);
            if (isMutable) {
                if (map.count(type) == 0) {
                    map[type] = std::move(executor);
                    return Success;
                } else {
                    return TypeAlreadyHasExecutor;
                }
            } else {
                return DispatcherIsImmutable;
            }
        }

        void MakeImmutable() {
            std::unique_lock<std::mutex> mlock(mutableMutex);
            isMutable = false;
        }

        AddTaskResult AddTask(ITask *task) {
            if (isMutable) {
                return AddTaskResult::DispatcherIsMutable;
            } else {
                auto found = map.find(task->GetType());
                if (found != map.end()) {
                    return found->second.AddTask(std::shared_ptr<ITask>(task));
                } else {
                    return AddTaskResult::UnknownTaskType;
                }
            }
        }

    private:
        bool isMutable = true;
        std::mutex mutableMutex;
        std::unordered_map<TaskType, std::unique_ptr<ExecutorDispatcher>> map;
    };
}

