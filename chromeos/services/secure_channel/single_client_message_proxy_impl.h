// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_MESSAGE_PROXY_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_MESSAGE_PROXY_IMPL_H_

#include <string>

#include "base/macros.h"
#include "chromeos/services/secure_channel/channel_impl.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/secure_channel/single_client_message_proxy.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace chromeos {

namespace secure_channel {

// Concrete SingleClientMessageProxy implementation, which utilizes a
// ChannelImpl and MessageReceiverPtr to send/receive messages.
class SingleClientMessageProxyImpl : public SingleClientMessageProxy,
                                     public ChannelImpl::Delegate {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetInstanceForTesting(Factory* factory);
    virtual ~Factory();
    virtual std::unique_ptr<SingleClientMessageProxy> BuildInstance(
        SingleClientMessageProxy::Delegate* delegate,
        const std::string& feature,
        mojom::ConnectionDelegatePtr connection_delegate_ptr);

   private:
    static Factory* test_factory_;
  };

  ~SingleClientMessageProxyImpl() override;

 private:
  friend class SecureChannelSingleClientMessageProxyImplTest;

  SingleClientMessageProxyImpl(
      SingleClientMessageProxy::Delegate* delegate,
      const std::string& feature,
      mojom::ConnectionDelegatePtr connection_delegate_ptr);

  // SingleClientMessageProxy:
  void HandleReceivedMessage(const std::string& feature,
                             const std::string& payload) override;
  void HandleRemoteDeviceDisconnection() override;

  // ChannelImpl::Delegate:
  void OnSendMessageRequested(const std::string& message,
                              base::OnceClosure on_sent_callback) override;
  const mojom::ConnectionMetadata& GetConnectionMetadata() override;
  void OnClientDisconnected() override;

  void FlushForTesting();

  const std::string feature_;
  mojom::ConnectionDelegatePtr connection_delegate_ptr_;
  std::unique_ptr<ChannelImpl> channel_;
  mojom::MessageReceiverPtr message_receiver_ptr_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientMessageProxyImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_MESSAGE_PROXY_IMPL_H_
