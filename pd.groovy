@Grab(group='org.apache.camel', module='camel-websocket', version='2.18.1')
@Grab(group='org.apache.camel', module='camel-ahc-ws', version='2.18.1')
@Grab(group='org.apache.camel', module='camel-bindy', version='2.18.1')
@Grab(group='org.apache.camel', module='camel-metrics', version='2.18.1')
@Grab('org.apache.camel:camel-core:2.18.1')
@Grab('org.slf4j:slf4j-simple:1.7.22')

import org.apache.camel.*
import org.apache.camel.impl.*
import org.apache.camel.builder.*
import org.apache.camel.dataformat.bindy.annotation.CsvRecord
import org.apache.camel.dataformat.bindy.annotation.DataField
import org.apache.camel.dataformat.bindy.csv.BindyCsvDataFormat
import org.apache.camel.util.toolbox.*
import org.apache.camel.processor.*
import org.apache.camel.processor.aggregate.AbstractListAggregationStrategy
import java.nio.ByteBuffer
import java.nio.ByteOrder

String uri = "ahc-ws://192.168.220.165:81";

def camelContext = new DefaultCamelContext()
// camelContext.setTracing(true);
BindyCsvDataFormat packetDataFormat = new BindyCsvDataFormat(Packet.class);

camelContext.addRoutes(new RouteBuilder() {
    def void configure() {
		from(uri)
		.routeId("ws received")
		.process(new Processor() {
			def void process(Exchange exchange) {
				Packet p = new Packet(exchange.getIn().getBody(byte[].class))
				exchange.getOut().setBody(p)
			}
		})
		.aggregate(constant(true), new ListOfPacketsStrategy())
        .completionSize(300)
		.marshal(packetDataFormat)
		.to('file://?fileName=pd.csv&fileExist=Append')
		.log('added 300 samples')
    }
})
camelContext.start()

addShutdownHook{ camelContext.stop() }
synchronized(this){
	this.wait()
}

@CsvRecord(separator=",", generateHeaderColumns=false)
class Packet {

	def final byte code;
    @DataField(pos = 1, precision = 6)
    def final float temperature;
    @DataField(pos = 2, precision = 8)
    def final float gyroscopeX;
    @DataField(pos = 3, precision = 8)
    def final float gyroscopeY;
    @DataField(pos = 4, precision = 8)
    def final float gyroscopeZ;
    @DataField(pos = 5, precision = 8)
    def final float accelerometerX;
    @DataField(pos = 6, precision = 8)
    def final float accelerometerY;
    @DataField(pos = 7, precision = 8)
    def final float accelerometerZ;
    @DataField(pos = 8, precision = 8)
    def final float magnetometerX;
    @DataField(pos = 9, precision = 8)
    def final float magnetometerY;
    @DataField(pos = 10, precision = 8)
    def final float magnetometerZ;
    @DataField(pos = 11, precision = 8)
    def final int vcc;
    @DataField(pos = 12)
    def final int uptime;
    @DataField(pos = 13)
    def final long time;

    public Packet(byte[] bytes) {
    	try {
			ByteBuffer bb = ByteBuffer.wrap(bytes);
			bb.order(ByteOrder.LITTLE_ENDIAN);
			code = bb.getInt()
			if (code == 9) {
				gyroscopeX = bb.getFloat();
				gyroscopeY = bb.getFloat();
				gyroscopeZ = bb.getFloat();
				accelerometerX = bb.getFloat();
				accelerometerY = bb.getFloat();
				accelerometerZ = bb.getFloat();
				magnetometerX = bb.getFloat();
				magnetometerY = bb.getFloat();
				magnetometerZ = bb.getFloat();
				vcc = bb.getFloat() / 1000.0;
				uptime = bb.getInt();
				time = bb.getInt();
				temperature = bb.getFloat();
			}
    	} catch (Exception e) {
    		println 'please check your data'
    	}
    }

    String toString() {
    	"""code: $code uptime: $uptime time: ${new Date(time*1000)} vcc: $vcc temperature: $temperature
    	gyroscopeX: $gyroscopeX gyroscopeY: $gyroscopeY gyroscopeZ: $gyroscopeZ
    	accelerometerX: $accelerometerX accelerometerY: $accelerometerY accelerometerZ: $accelerometerZ
    	magnetometerX: $magnetometerX magnetometerY: $magnetometerY magnetometerZ: $magnetometerZ"""
    }
}

public final class ListOfPacketsStrategy extends AbstractListAggregationStrategy<Packet> {
	@Override
	public Packet getValue(Exchange exchange) {
		return exchange.getIn().getBody(Packet.class);
	}
}