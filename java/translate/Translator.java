package translate;

import java.util.LinkedList;

public class Translator {
	private TranslatorPrivate impl;
	private ir.IREnvironment IRenv;
	private ir.Function program_body; 
	private frame.AbstractFrameManager framemanager;
	
	public Translator(ir.IREnvironment ir_env,
			frame.AbstractFrameManager _framemanager) {
		IRenv = ir_env;
		impl = new TranslatorPrivate(ir_env, _framemanager);
		framemanager = _framemanager;
	}
	
	public void translateProgram(parse.Node expression) {
		frame.AbstractFrame frame = framemanager.newFrame(
				framemanager.rootFrame(), ".global");
		TranslatorPrivate.Result res = impl.translateProgram(expression, frame);
		program_body = new ir.Function(".program",
			new ir.StatmCode(IRenv.codeToStatement(res.code)), frame, null); 
	}
	
	public void canonicalize() {
		for (Function func: impl.getFunctions())
			if (func.implementation.body != null)
				impl.canonicalizeFunctionBody(func.implementation);
		impl.canonicalizeFunctionBody(program_body);
	}
	
	public ir.Function getProgramBody() {
		return program_body;
	}
	
	public LinkedList<ir.Function> getFunctions() {
		LinkedList<ir.Function> result = new LinkedList<>();
		for (Function func: impl.getFunctions())
			if (func.implementation.body != null)
				result.add(func.implementation);
		return result;
	}
}
