/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#ifndef THREADHELPER_H
#define THREADHELPER_H

#include <QObject>
#include <QMutex>
#include <QMetaObject>
#include <QWaitCondition>
#include <functional>
#include <QThread>
#include <AzCore/Memory/SystemAllocator.h>

namespace AssetProcessor
{
    class ThreadWorker
        : public QObject
    {
        Q_OBJECT

    public:
        AZ_CLASS_ALLOCATOR(ThreadWorker, AZ::SystemAllocator, 0)
        explicit ThreadWorker(QObject* parent = 0)
            : QObject(parent)
        {
            m_runningThread = new QThread();
        }
        virtual ~ThreadWorker(){}
        //! Destroy function not only stops the running thread
        //! but also deletes the ThreadWorker object
        void Destroy()
        {
            // We are calling deletelater() before quiting the thread
            // because deletelater will only schedule the object for deletion
            // if the thread is running

            QThread* threadptr = m_runningThread;
            deleteLater();
            threadptr->quit();
            threadptr->wait();
            delete threadptr;
        }

Q_SIGNALS:

    public Q_SLOTS:
        void RunInThread()
        {
            create();
        }


    protected:
        virtual void create() = 0;

        QMutex m_waitConditionMutex;
        QWaitCondition m_waitCondition;
        QThread* m_runningThread;
    };

    //! This class helps in creating an instance of the templated
    //! QObject class in a new thread.
    //! Intialize is a blocking call and than will only return with a pointer to the
    //! new object only after the object is created in the new thread.
    //! Please note that each instance of this class has to be dynamically allocated.
    template<typename T>
    class ThreadController
        : public ThreadWorker
    {
    public:
        AZ_CLASS_ALLOCATOR(ThreadController<T>, AZ::SystemAllocator, 0)
        typedef std::function<T* ()> FactoryFunctionType;

        ThreadController()
            : ThreadWorker()
        {
            m_runningThread->start();
            this->moveToThread(m_runningThread);
        }

        virtual ~ThreadController()
        {
        }

        T* initialize(FactoryFunctionType callback = nullptr)
        {
            m_function = callback;
            m_waitConditionMutex.lock();
            QMetaObject::invokeMethod(this, "RunInThread", Qt::QueuedConnection);

            m_waitCondition.wait(&m_waitConditionMutex);
            m_waitConditionMutex.unlock();
            return m_instance;
        }

        virtual void create() override
        {
            if (m_function)
            {
                m_instance = m_function();
            }

            m_waitCondition.wakeOne();
        }

    private:
        T* m_instance;
        FactoryFunctionType m_function;
    };
}

#endif // THREADHELPER_H

