/* -*- Mode: c; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/*
 * Copyright (C) 2020 HBK – Hottinger Brüel & Kjær
 * Skodsborgvej 307
 * DK-2850 Nærum
 * Denmark
 * http://www.hbkworld.com
 * All rights reserved
 *
 * The copyright to the computer program(s) herein is the property of
 * HBK – Hottinger Brüel & Kjær (HBK), Denmark. The program(s)
 * may be used and/or copied only with the written permission of HBK
 * or in accordance with the terms and conditions stipulated in the
 * agreement/contract under which the program(s) have been supplied.
 * This copyright notice must not be removed.
 *
 * This Software is licensed by the
 * "General supply and license conditions for software"
 * which is part of the standard terms and conditions of sale from HBK.
 */

#include <fstream>
#include <functional>


#include "boost/asio/ip/tcp.hpp"
#include "boost/beast.hpp"

#include <json/value.h>
#include <json/writer.h>

//#include "msservice/msservicedatatypes.hpp"
#include "objectmodel/ObjectModelConstants.hpp"

#include "jet/peerasync.hpp"

#include "fbproxy/fbproxy.hpp"
#include "hbm/string/split.h"

#include "server.h"
#include "Session.h"
#include "LocalSession.h"
#include "TcpSession.h"
#include "WebsocketSession.h"

namespace Hbk::Streaming {
    
    /// Maximum number of sessions allowed
    static const unsigned int maxWorkerCount = 8;
    static const char signalsContainsFilter[] = "signals/signal";
    
    Server::Server(boost::asio::io_context& readerIoContext, hbm::jet::PeerAsync &jetPeer, const std::string& ringbufferFactoryName, uint16_t tcpDataPort, uint16_t wsDataPort, const std::string& localEndPointFile, const std::string& conntrolUrlPath)
        : m_readerIoContext(readerIoContext)
        , m_jetPeer(jetPeer)
        , m_ringbufferFactoryName(ringbufferFactoryName)
        , m_tcpDataPort(tcpDataPort)
        , m_websocketDataPort(wsDataPort)
        , m_controlUrlPath(conntrolUrlPath)
        , m_tcpAcceptor(readerIoContext, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), m_tcpDataPort))
        , m_websocketTcpAcceptor(readerIoContext, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), m_websocketDataPort))
        // placing a '\0' at the beginning creates an abstract unix domain socket.
        // See man page (man 7 unix) for details 
        , m_localAcceptor(readerIoContext, std::string("\0", 1) + std::string(localEndPointFile))
    {
        hbm::jet::matcher_t signalMatchFb;
        hbm::jet::matcher_t signalMatchSignal;
        
        // variant that looks into functionblocks and searches for public output ports
        signalMatchFb.startsWith = Hbk::Fb::FB_PATH_PREFIX;
        signalMatchSignal.contains = signalsContainsFilter;
        m_fbFetchId = m_jetPeer.addFetchAsync(signalMatchFb, std::bind(&Server::fetchPublicPortsCb, this, std::placeholders::_1, std::placeholders::_2));
        m_signalFetchId = m_jetPeer.addFetchAsync(signalMatchSignal, std::bind(&Server::fetchSignalModeCb, this, std::placeholders::_1, std::placeholders::_2));
        
        m_availablePath = STREAM_PATH_PREFIX + "available";
        try {
            m_jetPeer.addStateAsync(m_availablePath, Json::arrayValue, hbm::jet::responseCallback_t(), hbm::jet::stateCallback_t());
        } catch (...) {
            syslog(LOG_ERR, "Stream %s: Exception when adding state", m_availablePath.c_str());
        }
        
        // Open functionblock driver interface! (To avoid page faults to happen later)
        Hbk::Fb::driver::instance();
    }
    
    Server::~Server() {
        // Remove service state. Will be collected by system service to update avahi
        m_jetPeer.removeStateAsync(m_tcpServicePath, hbm::jet::responseCallback_t());
        m_jetPeer.removeStateAsync(m_wsServicePath, hbm::jet::responseCallback_t());
        
        m_jetPeer.removeStateAsync(m_availablePath);
        
        m_jetPeer.removeFetchAsync(m_fbFetchId);
        m_jetPeer.removeFetchAsync(m_signalFetchId);
    }
    
    void Hbk::Streaming::Server::createServiceStates()
    {
        {
            Json::Value serviceTcp;
            std::string serviceId = objectmodel::constants::serviceIdStreamingTcp;
            serviceTcp[objectmodel::constants::avahiServicePort] = m_tcpDataPort;
            m_tcpServicePath = objectmodel::constants::avahiBasePath + serviceId;
            m_jetPeer.addStateAsync(m_tcpServicePath, serviceTcp, hbm::jet::responseCallback_t(), hbm::jet::stateCallback_t());
        }
        
        {
            Json::Value serviceWebSocket;
            std::string serviceId = objectmodel::constants::serviceIdStreamingWebsocket;
            serviceWebSocket[objectmodel::constants::avahiServicePort] = m_websocketDataPort;
            serviceWebSocket[objectmodel::constants::avahiServiceTextRecords]["path"] = "/";
            m_wsServicePath = objectmodel::constants::avahiBasePath + serviceId;
            m_jetPeer.addStateAsync(m_wsServicePath, serviceWebSocket, hbm::jet::responseCallback_t(), hbm::jet::stateCallback_t());
        }
    }
    
    int Server::start()
    {
        createServiceStates();
        
        syslog(LOG_INFO, "Starting");
        startTcpAccept();
        startWebsocketAccept();
        startLocalAccept();
        return 0;
    }
    
    void Server::stop()
    {
        syslog(LOG_INFO, "Stopping");
        {
            std::lock_guard < std::mutex > lock(m_workersMtx);
            for (auto& iter: m_workers) {
                // check whether the weak pointer is still valid. Own it for some time to stop the worker and release again
                if (auto sharedPointer = iter.second.lock()) {
                    sharedPointer->stop();
                }
            }
            m_workers.clear();
        }
    }
    
    
    
    void Server::fetchPublicPortsCb(const Json::Value &params, int status)
    {
        if (status<0) {
            return;
        }
        Json::Value outputsNode = params[hbm::jet::VALUE][Hbk::Fb::CONNECTIONS][Hbk::Fb::OUTPUTS];
        std::string event = params[hbm::jet::EVENT].asString();
        std::string path = params[hbm::jet::PATH].asString();
        
        SignalIds removedSignals;
        SignalIds addedSignals;
        
        if ((event == hbm::jet::CHANGE)||(event == hbm::jet::ADD)){
            // Check if the a signal is directly visibly on jet and not includes in a Connections.
            // A Signal has the reserved key value Value as a signalId.
            if (params[hbm::jet::VALUE][Hbk::Fb::PUBLIC].isBool()) {
                bool isPublic = params[hbm::jet::VALUE][Hbk::Fb::PUBLIC].asBool();
                std::string signalId = path + Hbk::Fb::SIGNALID_SEPARATOR + Hbk::Fb::VALUE;
                if (isPublic) {
                    // has to be added if it does not exist already
                    std::pair<SignalIds::iterator,bool > result;
                    result = m_availableSignals.insert(signalId);
                    if (result.second) {
                        addedSignals.insert(signalId);
                    }
                } else {
                    if (m_availableSignals.erase(signalId)) {
                        removedSignals.insert(signalId);
                    }
                } 
            } else {
                for (Json::Value::const_iterator outputsNodeIter = outputsNode.begin() ; outputsNodeIter != outputsNode.end() ; outputsNodeIter++ ) {
                    std::string portId = outputsNodeIter.key().asString();
                    std::string signalId = path + Hbk::Fb::SIGNALID_SEPARATOR + portId;
                    
                    bool isPublic = (*outputsNodeIter)[Hbk::Fb::PUBLIC].asBool();
                    if (isPublic) {
                        // has to be added if it does not exist already
                        std::pair<SignalIds::iterator,bool > result;
                        result = m_availableSignals.insert(signalId);
                        if (result.second) {
                            addedSignals.insert(signalId);
                        }
                    } else {
                        // has to be removed because it is not public anymore!
                        if (m_availableSignals.erase(signalId)) {
                            removedSignals.insert(signalId);
                        }
                    }
                }
            }
        } else if (event == hbm::jet::REMOVE) {
            // since this function disappeared, all signals are to be removed
            if (params[hbm::jet::VALUE][Hbk::Fb::PUBLIC].isBool()) {
                std::string signalId = path + Hbk::Fb::SIGNALID_SEPARATOR + Hbk::Fb::VALUE;
                if (m_availableSignals.erase(signalId)) {
                    removedSignals.insert(signalId);
                }
            } else {
                for (Json::Value::const_iterator outputsNodeIter = outputsNode.begin() ; outputsNodeIter != outputsNode.end() ; outputsNodeIter++ ) {
                    std::string portId = outputsNodeIter.key().asString();
                    std::string signalId = path + Hbk::Fb::SIGNALID_SEPARATOR + portId;
                    if (m_availableSignals.erase(signalId)) {
                        removedSignals.insert(signalId);
                    }
                }
            }
        }
        
        updateAvailableSignals(removedSignals, addedSignals);
    }


    void Server::fetchSignalModeCb(const Json::Value &params, int status)
    {
        if (status<0) {
            return;
        }
        Json::Value outputsNode = params[hbm::jet::VALUE][Hbk::Fb::CONNECTIONS][Hbk::Fb::OUTPUTS];
        std::string event = params[hbm::jet::EVENT].asString();
        std::string path = params[hbm::jet::PATH].asString();
        
        SignalIds removedSignals;
        SignalIds addedSignals;
        hbm::string::tokens tok = hbm::string::split(path, objectmodel::constants::idSeparator);
        std::string functionBlockName;
        if (!tok.empty()) {
            functionBlockName = tok[tok.size() - 1];
        } else {
            functionBlockName = path;
        }

 

        if ((event == hbm::jet::CHANGE)||(event == hbm::jet::ADD)){
            // Check if the a signal is directly visibly on jet and not includes in a Connections.
            // A Signal has the reserved key value Value as a signalId.
            // /connectors/connector1/inputStage/channels/channel/signals/signal1
            
            if (functionBlockName.rfind("signal", 0) == 0) {
                if (params[hbm::jet::VALUE]["source"].isUInt()) {
                    std::string signalId = path + Hbk::Fb::SIGNALID_SEPARATOR + "value";
                    // has to be added if it does not exist already
                    std::pair<SignalIds::iterator,bool > result;
                    result = m_availableSignals.insert(signalId);
                    if (result.second) {
                        addedSignals.insert(signalId);
                    }
                }
            }
        } else if (event == hbm::jet::REMOVE)    {
            // since this function disappeared, all signals are to be removed
            if (functionBlockName.rfind("signal", 0) == 0) {
                std::string signalId = path + Hbk::Fb::SIGNALID_SEPARATOR + "value";
                if (m_availableSignals.erase(signalId)) {
                    removedSignals.insert(signalId);
                }
            } else {
                for (Json::Value::const_iterator outputsNodeIter = outputsNode.begin() ; outputsNodeIter != outputsNode.end() ; outputsNodeIter++ ) {
                    std::string portId = outputsNodeIter.key().asString();
                    std::string signalId = path + Hbk::Fb::SIGNALID_SEPARATOR + portId;
                    if (m_availableSignals.erase(signalId)) {
                        removedSignals.insert(signalId);
                    }
                }
            }
        }
        
        updateAvailableSignals(removedSignals, addedSignals);
    }
    
    void Server::updateAvailableSignals(const SignalIds& removedSignals, const SignalIds& addedSignals)
    {
        if (removedSignals.empty() && addedSignals.empty()) {
            // no relevant changes!
            return;
        }
        
        std::lock_guard < std::mutex > lock(m_workersMtx);
        for (auto& iter: m_workers) {
            // send meta information informing about available/unavailable signals
            
            // check whether the weak pointer is still valid. Own it for some time to do the work and release again
            if (auto sharedPointer = iter.second.lock()) {
                if (!addedSignals.empty()) {
                    sharedPointer->addRemoveSignalIds(addedSignals, true);
                }
                
                if (!removedSignals.empty()) {
                    sharedPointer->addRemoveSignalIds(removedSignals, false);
                }
            } else {
                m_workers.erase(iter.first);
            }
        }
        
        Json::Value availableSignalsJson(Json::arrayValue);
        for(const auto& iter : m_availableSignals) {
            availableSignalsJson.append(iter);
        }
        
        m_jetPeer.notifyState(m_availablePath, availableSignalsJson);
    }
    
    void Server::createWorker(std::unique_ptr < Session > session)
    {
        /// Used to remove weak pointer to destroyed worker form our container of known workers.
        auto destructCb = [this](const std::string& streamId) {
            std::lock_guard < std::mutex > lock(m_workersMtx);
            m_workers.erase(streamId);
        };
        std::shared_ptr < Worker > newWorker = Worker::create(m_readerIoContext, m_jetPeer, std::move(session), destructCb);
        if (newWorker->start(m_ringbufferFactoryName, m_controlUrlPath) == -1) {
            return;
        }
        
        // send meta information with all available signal ids
        if (!m_availableSignals.empty()) {
            newWorker->addRemoveSignalIds(m_availableSignals, true);
        }
        {
            std::lock_guard < std::mutex > lock(m_workersMtx);
            m_workers[newWorker->id()] = newWorker;
        }
    }

    void Server::startTcpAccept()
    {
        auto handlTcpAccept = [this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket&& streamSocket) {
            if (!ec) {
                checkWorkerCount(std::make_unique < TcpSession > (std::move(streamSocket)));
            }
            startTcpAccept();
        };
        m_tcpAcceptor.async_accept(handlTcpAccept);
    }
    
    void Server::startWebsocketAccept()
    {
        m_websocketTcpAcceptor.async_accept(std::bind(&Server::handleWebsocketTcpAccept, this, std::placeholders::_1, std::placeholders::_2));
    }
    
    void Server::startLocalAccept()
    {
        auto localAcceptCb = [this](const boost::system::error_code& ec, boost::asio::local::stream_protocol::socket&& streamSocket) {
            if (!ec) {
                auto localSession = std::make_unique < LocalSession > (std::move(streamSocket));
                checkWorkerCount(std::move(localSession));
            }
            startLocalAccept();
        };
        m_localAcceptor.async_accept(localAcceptCb);
    }
    
    void Server::handleWebsocketTcpAccept(const boost::system::error_code& ec, boost::asio::ip::tcp::socket&& streamSocket)
    {
        if (!ec) {
            //std::shared_ptr < boost::beast::websocket::stream < boost::asio::ip::tcp::socket > > websocketStream = std::make_shared < boost::beast::websocket::stream < boost::asio::ip::tcp::socket > > (std::move(streamSocket));
            std::shared_ptr < boost::beast::websocket::stream <boost::beast::tcp_stream > > websocketStream = std::make_shared < boost::beast::websocket::stream <boost::beast::tcp_stream> > (std::move(streamSocket));
            
            // When accepting the websocket handshake, ownership of the websocket is transferred to the worker
            // need to provide parameters as copies since this context will disappear...
            auto handshakeHandler = [this, websocketStream](const boost::system::error_code& ec)
            {
                if (!ec) {
                    checkWorkerCount(std::make_unique < WebsocketSession > (websocketStream));
                } else {
                    std::cerr << "websocket handshake unsuccessful, ec: " << ec << std::endl;
                }
            };
            websocketStream->async_accept(handshakeHandler);
        }
        startWebsocketAccept();
    }
    
    void Server::checkWorkerCount(std::unique_ptr<Session> session)
    {
        if (m_workers.size() >= maxWorkerCount) {
            auto asyncCloseCb = [](const boost::system::error_code&)
            {
            };
            session->async_close(asyncCloseCb);
            /// \todo send an error stating "possible number of sessions exceeded..." or something similar.
            return;
        }
        createWorker(std::move(session));
    }
}
