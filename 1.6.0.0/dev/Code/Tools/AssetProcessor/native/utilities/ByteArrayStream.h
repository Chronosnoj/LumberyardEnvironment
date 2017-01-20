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
#ifndef ASSETBUILDER_BYTEARRAYSTREAM
#define ASSETBUILDER_BYTEARRAYSTREAM

#include <AzCore/IO/GenericStreams.h>
#include <AzFramework/Asset/AssetProcessorMessages.h>

#include <QByteArray>

namespace AssetProcessor
{
    //! Wrap a QByteArray (which has an int-interface) in a GenericStream.
    class ByteArrayStream
        : public AZ::IO::GenericStream
    {
    public:
        ByteArrayStream();
        ByteArrayStream(QByteArray* other); // attach to external
        ByteArrayStream(const char* data, unsigned int length); // const attach to read-only buffer

        bool IsOpen() const override
        {
            return true;
        }
        bool CanSeek() const override
        {
            return true;
        }

        void Seek(AZ::IO::OffsetType bytes, SeekMode mode) override;
        AZ::IO::SizeType Read(AZ::IO::SizeType bytes, void* oBuffer) override;
        AZ::IO::SizeType Write(AZ::IO::SizeType bytes, const void* iBuffer) override;
        AZ::IO::SizeType GetCurPos() const override;
        AZ::IO::SizeType GetLength() const override;

        QByteArray GetArray() const; // bytearrays are copy-on-write so retrieving it is akin to retreiving a refcounted object, its cheap to 'copy'

        void Reserve(int amount); // for performance.

    private:
        QByteArray* m_activeArray;
        QByteArray m_ownArray; // used when not constructed around an attached array
        bool m_usingOwnArray = true; // if false, its been attached
        int m_currentPos = 0; // the byte array underlying has only ints :(
        bool m_readOnly = false;
    };

    // Pack any serializable type into a QByteArray
    // note that this is not a specialization of the AZFramework version of this function
    // because C++ does not support partial specialization of function templates, only classes.
    template <class Message>
    bool PackMessage(const Message& message, QByteArray& buffer)
    {
        ByteArrayStream byteStream(&buffer);
        return AZ::Utils::SaveObjectToStream(byteStream, AZ::DataStream::ST_BINARY, &message, message.RTTI_GetType());
    }

    // Unpack any serializable type from a QByteArray
    // note that this is not a specialization of the AZFramework version of this function
    // because C++ does not support partial specialization of function templates, only classes.
    template <class Message>
    bool UnpackMessage(const QByteArray& buffer, Message& message)
    {
        ByteArrayStream byteStream(buffer.constData(), buffer.size());
        return AZ::Utils::LoadObjectFromStreamInPlace(byteStream, message);
    }
}



#endif // ASSETBUILDER_BYTEARRAYSTREAM
