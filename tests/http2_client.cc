#include "sylar/sylar.h"
#include "sylar/http2/http2_connection.h"

std::string bigstr;

void test() {
    bigstr = "aaaaaaaaaa";
    for(int i = 0; i < 1; ++i) {
        bigstr = bigstr + bigstr;
    }
    //sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress("192.168.1.6:8099");
    sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress("10.104.16.110:8099");
    sylar::http2::Http2Connection::ptr stream(new sylar::http2::Http2Connection());
    //if(!stream->connect(addr, true)) {
    if(!stream->connect(addr, false)) {
        std::cout << "connect " << *addr << " fail";
    }
    //if(stream->handleShakeClient()) {
    //    std::cout << "handleShakeClient ok," << stream->isConnected() << std::endl;
    //} else {
    //    std::cout << "handleShakeClient fail" << std::endl;
    //    return;
    //}
    stream->start();
    //sleep(1);

    for(int x = 0; x < 30; ++x) {
        sylar::IOManager::GetThis()->schedule([stream, x](){
            for(int i = 0; i < 10240; ++i) {
                sylar::http::HttpRequest::ptr req(new sylar::http::HttpRequest);
                //req->setHeader(":path", "/");
                //req->setHeader(":method", "GET");
                //req->setHeader(":scheme", "http");
                //req->setHeader(":host", "127.0.0.1");

                req->setHeader(":method", "GET");
                req->setHeader(":scheme", "http");
                req->setHeader(":path", "/_/config?abc=111#cde");
                req->setHeader(":authority", "127.0.0.1:8090");
                req->setHeader("content-type", "text/html");
                //req->setHeader("content-encoding", "gzip");
                req->setHeader("user-agent", "grpc-go/1.37.0");
                req->setHeader("hello", "world");
                req->setHeader("id", std::to_string(x) + "_" + std::to_string(i));

                auto body = bigstr + "_hello";
                //auto zs = sylar::ZlibStream::CreateGzip(true);
                //zs->write(body.c_str(), body.size());
                //zs->flush();
                //zs->getResult().swap(body);
                req->setBody(body);

                auto rt = stream->request(req, 100000);
                //std::cout << "----" << rt->result << std::endl;
                //std::cout << "bigstr.length=" << bigstr.length() << std::endl;
                std::cout << "----" << rt->toString() << std::endl;
                //sleep(1);
            }
        });
    }
}

int main(int argc, char** argv) {
    sylar::IOManager iom;
    iom.schedule(test);
    iom.addTimer(1000, [](){}, true);
    iom.stop();
    return 0;
}
