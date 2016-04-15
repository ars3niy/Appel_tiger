package util;

import java.io.PrintWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.util.TreeMap;

public class DebugPrinter {
	public static final boolean ENABLE = false;
	
	private static TreeMap<String, PrintWriter> open_files = new TreeMap<>();
	
	private PrintWriter f = null;
	
	public DebugPrinter(String filename) {
		if (! open_files.containsKey(filename)) {
			try {
				f = new PrintWriter(new File(filename));
				open_files.put(filename, f);
			} catch (FileNotFoundException e) {}
		} else
			f = open_files.get(filename);
	}
	
	public void debug(String message) {
		if (f != null) {
			f.println(message);
			f.flush();
		}
	}
	
	private static void preprint(PrintWriter out, int indent, String prefix)
	{
		for (int i = 0; i < indent; i++)
			out.print(' ');;
		out.print(prefix);
	}

	private static final String BINARYOPNAMES[] = {
		"OP_PLUS",
		"OP_MINUS",
		"OP_MUL",
		"OP_DIV",
		"OP_AND",
		"OP_OR",
		"OP_XOR",
		"OP_SHL",
		"OP_SHR",
		"OP_SHAR",
	};

	private static final String COMPARNAMES[] = {
		"OP_EQUAL",
		"OP_NONEQUAL",
		"OP_LESS",
		"OP_LESSEQUAL",
		"OP_GREATER",
		"OP_GREATEQUAL",
		"OP_ULESS",
		"OP_ULESSEQUAL",
		"OP_UGREATER",
		"OP_UGREATEQUAL"
	};

	private static void PrintExpression(PrintWriter out, ir.Expression exp,
		int indent, String prefix)
	{
		preprint(out, indent, prefix);
		if (exp instanceof ir.IntegerExp)
			out.printf("%d\n", ((ir.IntegerExp)exp).value);
		else if (exp instanceof ir.LabelAddressExp)
			out.printf("Label address %s\n",
				((ir.LabelAddressExp)exp).label().getName());
		else if (exp instanceof ir.RegisterExp) {
			if (((ir.RegisterExp)exp).reg.getName() == "")
				out.printf("Register %d\n", ((ir.RegisterExp)exp).reg.getIndex());
			else
				out.printf("Register %s\n", ((ir.RegisterExp)exp).reg.getName());
		} else if (exp instanceof ir.BinaryOpExp) { 
			out.printf("%s\n", BINARYOPNAMES[((ir.BinaryOpExp)exp).operation]);
			PrintExpression(out, ((ir.BinaryOpExp)exp).left, indent+4, "Left: ");
			PrintExpression(out, ((ir.BinaryOpExp)exp).right, indent+4, "Right: ");
		} else if (exp instanceof ir.MemoryExp) {
			out.printf("Memory value\n");
			PrintExpression(out, ((ir.MemoryExp)exp).address, indent+4, "Address: ");
		} else if (exp instanceof ir.CallExpression) { 
			out.printf("Function call\n");
			if (((ir.CallExpression)exp).callee_parentfp != null)
				PrintExpression(out, ((ir.CallExpression)exp).callee_parentfp, indent+4, "Parent FP parameter: ");
			PrintExpression(out, ((ir.CallExpression)exp).function, indent+4, "Call address: ");
			for (ir.Expression arg: ((ir.CallExpression)exp).arguments)
				 PrintExpression(out, arg, indent+4, "Argument: ");
		} else if (exp instanceof ir.StatExpSequence) { 
			out.printf("Statement and expression\n");
			PrintStatement(out, ((ir.StatExpSequence)exp).stat, indent+4, "Pre-statement: ");
			PrintExpression(out, ((ir.StatExpSequence)exp).exp, indent+4, "Value: ");
		} else {
			out.println("WTF");
		}
	}

	private static void PrintStatement(PrintWriter out, ir.Statement statm, int indent, String prefix)
	{
		preprint(out, indent, prefix);
		if (statm instanceof ir.MoveStatm) {
			out.printf("Move (assign)\n");
			PrintExpression(out, ((ir.MoveStatm)statm).to, indent+4, "To: ");
			PrintExpression(out, ((ir.MoveStatm)statm).from, indent+4, "From: ");
		} else if (statm instanceof ir.ExpressionStatm) {
			out.printf("Expression, ignore result\n");
			PrintExpression(out, ((ir.ExpressionStatm)statm).exp, indent+4, "To: ");
		} else if (statm instanceof ir.JumpStatm) {
			out.printf("Unconditional jump\n");
			PrintExpression(out, ((ir.JumpStatm)statm).dest, indent+4, "Address: ");
		} else if (statm instanceof ir.CondJumpStatm) {
			out.printf("Conditional jump\n");
			preprint(out, indent+4, "Comparison: ");
			out.printf("%s\n", COMPARNAMES[((ir.CondJumpStatm)statm).comparison]);
			PrintExpression(out, ((ir.CondJumpStatm)statm).left, indent+4, "Left operand: ");
			PrintExpression(out, ((ir.CondJumpStatm)statm).right, indent+4, "Right operand: ");
			preprint(out, indent+4, "Label if true: ");
			if (((ir.CondJumpStatm)statm).trueDest() == null)
				out.printf("UNKNOWN\n");
			else
				out.printf("%s\n", ((ir.CondJumpStatm)statm).trueDest().getName());
			preprint(out, indent+4, "Label if false: ");
			if (((ir.CondJumpStatm)statm).falseDest() == null)
				out.printf("UNKNOWN\n");
			else
				out.printf("%s\n", ((ir.CondJumpStatm)statm).falseDest().getName());
		} else if (statm instanceof ir.StatmSequence) {
			out.printf("Sequence\n");
			for (ir.Statement p: ((ir.StatmSequence)statm).statements)
				 PrintStatement(out, p, indent+4, "");
		} else if (statm instanceof ir.LabelPlacementStatm) {
			out.printf("Label here: %s\n", ((ir.LabelPlacementStatm)statm).label.getName());
		} else {
			out.println("WTF");
		}
	}
	
	public static void PrintFunction(PrintWriter out, ir.Function func) {
		int indent = 0;
		if (func.label != null) {
			out.printf("Function %s\n", func.label.getName());
			indent = 4;
		}
		if (func.body instanceof ir.ExpCode)
			PrintExpression(out, ((ir.ExpCode)func.body).exp, indent, "");
		else if (func.body instanceof ir.StatmCode)
			PrintStatement(out, ((ir.StatmCode)func.body).statm, indent, "");
	}
	
	public static void PrintBlob(PrintWriter out, ir.Blob blob) {
		out.printf("Label here: %s\n", blob.label.getName());
		if (blob.data.length == 0)
			out.print("(0 bytes)");
		else {
			out.print(' ');
			for (byte c: blob.data) {
				out.printf("0x%x ", (int)c);
			}
		}
		out.print(" \"");
		for (byte c: blob.data)
			out.print((char)c);
		out.println("\"");
	}
}
