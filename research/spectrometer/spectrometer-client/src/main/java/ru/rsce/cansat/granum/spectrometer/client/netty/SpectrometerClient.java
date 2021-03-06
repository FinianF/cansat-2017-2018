package ru.rsce.cansat.granum.spectrometer.client.netty;


import io.netty.bootstrap.Bootstrap;
import io.netty.channel.Channel;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.ChannelOption;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.nio.NioSocketChannel;

import ru.rsce.cansat.granum.spectrometer.client.netty.MessageDecoder;
import ru.rsce.cansat.granum.spectrometer.client.netty.MessageHandler;

public class SpectrometerClient {
	
	public static interface ClientMessageListener {
		public void onSpectrometerMessage(Message msg) throws Exception;
	}
	

	public void start(String host, int port) throws Exception {
		workerGroup = new NioEventLoopGroup();
		
		try {
			Bootstrap b = new Bootstrap()
				.group(workerGroup)
				.channel(NioSocketChannel.class)
				.option(ChannelOption.SO_KEEPALIVE, true)
				.handler(new ChannelInitializer<Channel>() {
					@Override
					protected void initChannel(Channel ch) throws Exception {
						ch.pipeline().addLast(new MessageDecoder());
						ch.pipeline().addLast(new MessageHandler(SpectrometerClient.this));
					}
				});
			
			ChannelFuture f = b.connect(host, port).sync();
			ch = f.channel();
			ch.read();
			ch.closeFuture().addListener(new ChannelFutureListener() {

				@Override
				public void operationComplete(ChannelFuture future) throws Exception {
					workerGroup.shutdownGracefully();
				}
			});
			
		} catch (Exception e){
			workerGroup.shutdownGracefully();
			throw e;
		}
	}
	
	
	public void waitExit() throws Exception {
		ch.closeFuture().sync();
	}
	
	
	public void setMsgListener(ClientMessageListener listener_) {
		msgListener = listener_;
	}
	

	public void pushMessage(Message msg) throws Exception {
		if (msgListener != null)
			msgListener.onSpectrometerMessage(msg);
	}

	
	private EventLoopGroup workerGroup;
	private Channel ch;
	
	private ClientMessageListener msgListener;
}
